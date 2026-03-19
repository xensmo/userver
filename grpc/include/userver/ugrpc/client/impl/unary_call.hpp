#pragma once

#include <fmt/format.h>
#include <google/protobuf/message.h>
#include <grpcpp/support/async_unary_call.h>

#include <userver/engine/sleep.hpp>
#include <userver/server/request/task_inherited_data.hpp>
#include <userver/tracing/tags.hpp>
#include <userver/utils/expected.hpp>
#include <userver/utils/fast_scope_guard.hpp>
#include <userver/utils/impl/internal_tag.hpp>

#include <userver/ugrpc/client/call_context.hpp>
#include <userver/ugrpc/client/client_qos.hpp>
#include <userver/ugrpc/client/completion_status.hpp>
#include <userver/ugrpc/client/exceptions.hpp>
#include <userver/ugrpc/client/impl/async_methods.hpp>
#include <userver/ugrpc/client/impl/call_params.hpp>
#include <userver/ugrpc/client/impl/call_state.hpp>
#include <userver/ugrpc/client/impl/middleware_pipeline.hpp>
#include <userver/ugrpc/client/impl/prepare_async_call.hpp>
#include <userver/ugrpc/client/impl/retry_backoff.hpp>
#include <userver/ugrpc/client/impl/tracing.hpp>
#include <userver/ugrpc/impl/async_method_invocation.hpp>
#include <userver/ugrpc/impl/status_utils.hpp>
#include <userver/ugrpc/status_codes.hpp>
#include <userver/ugrpc/time_utils.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client::impl {

template <typename Stub, typename Request, typename Response>
class UnaryCall final {
public:
    using PrepareUnaryCall = PrepareUnaryCallProxy<Stub, Request, Response>;

    UnaryCall(CallParams&& params, PrepareUnaryCall&& prepare_unary_call, const Request& request)
        : call_options_{std::move(params.call_options)},
          state_{std::move(params)},
          context_{utils::impl::InternalTag{}, state_},
          prepare_unary_call_{std::move(prepare_unary_call)},
          request_{request}
    {}

    ~UnaryCall() = default;

    UnaryCall(UnaryCall&&) = delete;
    UnaryCall& operator=(UnaryCall&&) = delete;

    CallContext& GetContext() noexcept { return context_; }
    const CallContext& GetContext() const noexcept { return context_; }

    void Perform() {
        const utils::FastScopeGuard span_scope([this]() noexcept { state_.ResetSpan(); });
        CallWithRetries();
    }

    Response&& ExtractResponse() {
        if (inherited_deadline_reached_) {
            USERVER_NAMESPACE::server::request::MarkTaskInheritedDeadlineExpired();
        }

        if (!completion_status_.has_value()) {
            ThrowSpecialCaseCompletionError(completion_status_.error());
        }

        if (!completion_status_.value().ok()) {
            ugrpc::client::ThrowErrorWithStatus(state_.GetCallName(), std::move(completion_status_).value());
        }

        return std::move(response_);
    }

    void Abandon() { abandoned_ = true; }

private:
    void CallWithRetries() {
        const utils::FastScopeGuard commit_state_guard([this]() noexcept { state_.Commit(); });

        const auto inherited_deadline = USERVER_NAMESPACE::server::request::GetTaskInheritedDeadline();
        const auto deadline = std::min(call_options_.GetDeadline(), inherited_deadline);
        const int max_attempts = call_options_.GetAttempts();
        state_.GetSpan().AddTag(tracing::kMaxAttempts, max_attempts);

        int attempt = 0;
        RetryBackoff retry_backoff;

        auto* retry_limiter = state_.GetRetryLimiter();

        while (!engine::current_task::ShouldCancel()) {
            ++attempt;
            state_.GetSpan().AddTag(tracing::kAttempts, attempt);
            impl::SetupClientContext(state_, call_options_);

            PerformAttempt();

            if (!completion_status_.has_value()) {
                switch (completion_status_.error()) {
                    case SpecialCaseCompletionType::kNetworkError:
                        OnInterrupted();
                        return;
                    case SpecialCaseCompletionType::kTimeoutDeadlinePropagated:
                        OnDone(grpc::Status{grpc::StatusCode::DEADLINE_EXCEEDED, "Propagated deadline exceeded"});
                        return;
                    case SpecialCaseCompletionType::kCancelled:
                    case SpecialCaseCompletionType::kAbandoned:
                        OnCancelled();
                        return;
                    default:
                        UINVARIANT(false, "Unknown SpecialCaseCompletionType");
                        return;
                }
            }

            auto& status = completion_status_.value();

            if (status.ok()) {
                OnDone(status);
                return;
            }

            const bool limiter_allows = !retry_limiter || retry_limiter->CanRetry();

            if (!limiter_allows || attempt >= max_attempts || !ugrpc::IsRetryable(status.error_code())) {
                OnDone(status);
                return;
            }

            const auto delay = retry_backoff.NextAttemptDelay();
            if (deadline.IsReachable() && deadline.TimeLeft() <= delay) {
                if (deadline == inherited_deadline) {
                    inherited_deadline_reached_ = true;
                }
                OnDone(status);
                return;
            }

            engine::InterruptibleSleepFor(delay);
        }

        completion_status_ = DetermineCancellationCompletionStatus();

        OnCancelled();
    }

    std::unique_ptr<grpc::ClientAsyncResponseReader<Response>> StartCall() {
        auto call = prepare_unary_call_(state_.GetStub(), &state_.GetClientContext(), request_, &state_.GetQueue());
        call->StartCall();
        return call;
    }

    void PerformAttempt() {
        RunStartCallHooks();

        auto response_reader = StartCall();

        auto wait_status = ugrpc::impl::AsyncMethodInvocation::WaitStatus::kError;
        {
            ugrpc::impl::AsyncMethodInvocation invocation;  // Calls WaitNonCancellable in the destructor
            completion_status_ = grpc::Status{};
            response_reader->Finish(&response_, &completion_status_.value(), invocation.GetCompletionTag());
            wait_status = invocation.Wait();

            // If the user cancelled the request, notify grpcpp as soon as possible
            // (we still must wait for the operation to complete, but grpcpp may finish faster)
            if (ugrpc::impl::AsyncMethodInvocation::WaitStatus::kCancelled == wait_status) {
                state_.GetClientContext().TryCancel();
            }
        }  // We must wait for the request to complete; only then we can access completion_status_ and destroy UnaryCall

        if (ugrpc::impl::AsyncMethodInvocation::WaitStatus::kOk == wait_status) {
            ugrpc::impl::ClampStatusCodeToValidRange(completion_status_.value());
        }

        UpdateCompletionStatus(wait_status);

        impl::RunMiddlewarePipeline(
            state_,
            MiddlewareHooks::FinishHooks(
                completion_status_,
                completion_status_.has_value() && completion_status_.value().ok() ? ToBaseMessage(&response_) : nullptr
            )
        );

        auto retry_limiter = state_.GetRetryLimiter();
        if (retry_limiter) {
            retry_limiter->AccountCompletion(completion_status_);
        }
    }

    void RunStartCallHooks() {
        impl::RunMiddlewarePipeline(state_, MiddlewareHooks::StartCallHooks(ToBaseMessage(&request_)));
    }

    void UpdateCompletionStatus(ugrpc::impl::AsyncMethodInvocation::WaitStatus wait_status)
    {
        switch (wait_status) {
            case ugrpc::impl::AsyncMethodInvocation::WaitStatus::kOk: {
                if (grpc::StatusCode::DEADLINE_EXCEEDED == completion_status_.value().error_code() &&
                    state_.IsDeadlinePropagated())
                {
                    completion_status_ = utils::unexpected{SpecialCaseCompletionType::kTimeoutDeadlinePropagated};
                }
                break;
            }
            case ugrpc::impl::AsyncMethodInvocation::WaitStatus::kError:
                // CompletionQueue returned ok=false. For Client-side Finish ok should always be true.
                // It signifies that RPC has interrupted in abnormal manner.
                // Do not attempt further operations on the RPC.
                completion_status_ = utils::unexpected{SpecialCaseCompletionType::kNetworkError};
                break;
            case ugrpc::impl::AsyncMethodInvocation::WaitStatus::kCancelled:
                completion_status_ = DetermineCancellationCompletionStatus();
                break;
            case ugrpc::impl::AsyncMethodInvocation::WaitStatus::kDeadline:
                UINVARIANT(false, "Waiting without timeout mustn't produce kDeadline");
                break;
            default:
                UINVARIANT(false, "Unknown WaitStatus");
                break;
        }
    }

    CompletionStatus DetermineCancellationCompletionStatus() {
        const auto inherited_deadline = USERVER_NAMESPACE::server::request::GetTaskInheritedDeadline();

        if (abandoned_) {
            return utils::unexpected{SpecialCaseCompletionType::kAbandoned};
        } else if (engine::current_task::CancellationReason() == engine::TaskCancellationReason::kDeadline &&
                   inherited_deadline.IsReached())
        {
            return utils::unexpected{SpecialCaseCompletionType::kTimeoutDeadlinePropagated};
        }
        return utils::unexpected{SpecialCaseCompletionType::kCancelled};
    }

    void OnDone(const grpc::Status& status) {
        impl::HandleCallStatistics(state_, status);
        impl::SetStatusForSpan(state_.GetSpan(), status);
    }

    void OnInterrupted() {
        std::string error_message = "Call interrupted, Network error";
        const auto debug_error_string = state_.GetClientContext().debug_error_string();
        if (!debug_error_string.empty()) {
            error_message += fmt::format("\nAdditional GRPC error information: {}", debug_error_string);
        }
        impl::SetErrorForSpan(state_.GetSpan(), error_message);
        state_.GetStatsScope().OnNetworkError();
        state_.GetStatsScope().Flush();
    }

    void OnCancelled() {
        if (abandoned_) {
            impl::SetErrorForSpan(state_.GetSpan(), "Call abandoned");
        } else {
            impl::SetErrorForSpan(state_.GetSpan(), "Call cancelled");
            state_.GetStatsScope().OnCancelled();
        }
        state_.GetStatsScope().Flush();
    }

    void ThrowSpecialCaseCompletionError(SpecialCaseCompletionType completion_case) {
        switch (completion_case) {
            case SpecialCaseCompletionType::kNetworkError:
                throw RpcInterruptedError(state_.GetCallName(), "UnaryCall");
            case SpecialCaseCompletionType::kCancelled:
            case SpecialCaseCompletionType::kAbandoned:
                throw RpcCancelledError{state_.GetCallName(), "UnaryCall"};
            case SpecialCaseCompletionType::kTimeoutDeadlinePropagated:
                throw DeadlineExceededError(
                    state_.GetCallName(),
                    grpc::Status{grpc::StatusCode::DEADLINE_EXCEEDED, "Propagated deadline exceeded"}
                );
            default:
                UINVARIANT(false, "Unknown SpecialCaseCompletionType");
                break;
        }
    }

    CallOptions call_options_;
    CallState state_;
    CallContext context_;

    PrepareUnaryCall prepare_unary_call_;
    const Request& request_;

    Response response_;
    CompletionStatus completion_status_;
    bool inherited_deadline_reached_{false};

    std::atomic<bool> abandoned_{false};
};

}  // namespace ugrpc::client::impl

USERVER_NAMESPACE_END
