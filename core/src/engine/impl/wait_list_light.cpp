#include "wait_list_light.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <fmt/format.h>

#include <concurrent/impl/fast_atomic.hpp>
#include <engine/task/sleep_state.hpp>
#include <engine/task/task_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/assert.hpp>
#include <userver/utils/underlying_value.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {
namespace {

struct alignas(8) AwaiterWithEpoch32 final {
    TaskContext* context{nullptr};
    SleepState::Epoch epoch{0};
};

struct alignas(16) AwaiterWithEpoch64 final {
    TaskContext* context{nullptr};
    SleepState::Epoch epoch{0};
    std::uint32_t padding_dont_use{0};
};

// Silence 'unused' warning.
static_assert(sizeof(AwaiterWithEpoch64::padding_dont_use) != 0);

using AwaiterWithEpoch = std::conditional_t<sizeof(void*) == 8, AwaiterWithEpoch64, AwaiterWithEpoch32>;

}  // namespace
}  // namespace engine::impl

USERVER_NAMESPACE_END

template <>
struct fmt::formatter<USERVER_NAMESPACE::engine::impl::AwaiterWithEpoch> {
    static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(USERVER_NAMESPACE::engine::impl::AwaiterWithEpoch awaiter, FormatContext& ctx) const {
        return fmt::format_to(
            ctx.out(),
            "({}, {})",
            fmt::ptr(awaiter.context),
            USERVER_NAMESPACE::utils::UnderlyingValue(awaiter.epoch)
        );
    }
};

USERVER_NAMESPACE_BEGIN

namespace engine::impl {
namespace {

TaskContext* const kSignaled = reinterpret_cast<TaskContext*>(1);

void DoWakeup(AwaiterWithEpoch awaiter) {
    LOG_TRACE() << "WakeupOne awaiter=" << fmt::to_string(awaiter) << " use_count=" << awaiter.context->UseCount();
    awaiter.context->Wakeup(TaskContext::WakeupSource::kWaitList, awaiter.epoch);
}

}  // namespace

struct WaitListLight::Impl final {
    concurrent::impl::FastAtomic<AwaiterWithEpoch> awaiter{AwaiterWithEpoch{}};
};

WaitListLight::WaitListLight() noexcept = default;

WaitListLight::~WaitListLight() {
    UASSERT_MSG(IsEmptyRelaxed(), "Someone is awaiting on WaitListLight while it's being destroyed");
}

void WaitListLight::Append(boost::intrusive_ptr<impl::TaskContext>&& context) noexcept {
    [[maybe_unused]] const bool was_signaled = GetSignalOrAppend(std::move(context));
    UASSERT_MSG(!was_signaled, "Signals cannot be used with plain Append");
}

bool WaitListLight::GetSignalOrAppend(boost::intrusive_ptr<TaskContext>&& context) noexcept {
    UASSERT(context);

    const AwaiterWithEpoch new_awaiter{context.get(), context->GetEpoch()};
    LOG_TRACE() << "Append awaiter=" << fmt::to_string(new_awaiter) << " use_count=" << context->UseCount();

    AwaiterWithEpoch expected{};
    // seq_cst is important for the "Append-Check-Wakeup" sequence.
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_seq_cst,
        std::memory_order_relaxed>(expected, new_awaiter);
    if (!success) {
        UASSERT_MSG(
            expected.context == kSignaled,
            fmt::format(
                "Attempting to wait in a single atomic Awaiter "
                "from multiple coroutines: new={} existing={}",
                new_awaiter,
                expected
            )
        );
        return true;
    }

    // Keep a reference logically stored in the WaitListLight to ensure that
    // WakeupOne can complete safely in parallel with the awaiting task being
    // cancelled, Remove-d and stopped.
    context.detach();

    return false;
}

void WaitListLight::WakeupOne() {
    // seq_cst is important for the "Append-Check-Wakeup" sequence.
    const auto old_awaiter = impl_->awaiter.exchange<std::memory_order_seq_cst>(AwaiterWithEpoch{});
    if (old_awaiter.context == nullptr) {
        return;
    }

    UASSERT_MSG(old_awaiter.context != kSignaled, "Use SetSignalAndWakeupOne for dealing with signals instead");

    const boost::intrusive_ptr<TaskContext> context{
        old_awaiter.context,
        /*add_ref=*/false
    };
    DoWakeup(old_awaiter);
}

void WaitListLight::SetSignalAndWakeupOne() {
    // seq_cst is important for the "Append-Check-Wakeup" sequence.
    const auto old_awaiter = impl_->awaiter.exchange<std::memory_order_seq_cst>(AwaiterWithEpoch{kSignaled, {}});
    if (old_awaiter.context == nullptr || old_awaiter.context == kSignaled) {
        return;
    }

    const boost::intrusive_ptr<TaskContext> context{
        old_awaiter.context,
        /*add_ref=*/false
    };
    DoWakeup(old_awaiter);
}

void WaitListLight::Remove(TaskContext& context) noexcept {
    const AwaiterWithEpoch expected{&context, context.GetEpoch()};

    auto old_awaiter = expected;
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_release,
        std::memory_order_relaxed>(old_awaiter, AwaiterWithEpoch{});

    if (!success) {
        UASSERT_MSG(
            old_awaiter.context == nullptr || old_awaiter.context == kSignaled,
            fmt::format(
                "An unexpected context is occupying the "
                "WaitListLight: expected={} actual={}",
                expected,
                old_awaiter
            )
        );
        return;
    }

    LOG_TRACE() << "Remove awaiter=" << fmt::to_string(expected) << " use_count=" << context.UseCount();
    intrusive_ptr_release(&context);
}

bool WaitListLight::GetAndResetSignal() noexcept {
    AwaiterWithEpoch expected{kSignaled, {}};
    const bool success = impl_->awaiter.compare_exchange_strong<
        std::memory_order_relaxed,
        std::memory_order_relaxed>(expected, AwaiterWithEpoch{});

    if (!success && expected.context != nullptr) {
        utils::AbortWithStacktrace(
            fmt::format("ResetSignal with an active awaiter is not allowed: awaiter={}", expected)
        );
    }

    return success;
}

bool WaitListLight::IsSignaled() const noexcept {
    const auto torn_awaiter = impl_->awaiter.LoadWithTearing();
    std::atomic_thread_fence(std::memory_order_acquire);
    return torn_awaiter.context == kSignaled;
}

bool WaitListLight::IsEmptyRelaxed() noexcept {
    const auto awaiter = impl_->awaiter.load<std::memory_order_relaxed>();
    return awaiter.context == nullptr || awaiter.context == kSignaled;
}

}  // namespace engine::impl

USERVER_NAMESPACE_END
