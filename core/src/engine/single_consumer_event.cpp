#include <userver/engine/single_consumer_event.hpp>

#include <engine/impl/wait_list_light.hpp>
#include <engine/task/task_context.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/assert.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine {

class SingleConsumerEvent::EventAwaitable final : public impl::AwaitableBase {
public:
    explicit EventAwaitable(SingleConsumerEvent& event)
        : event_(event)
    {}

    bool IsReady() const noexcept override { return event_.IsReady(); }

    void TryAppendAwaiter(boost::intrusive_ptr<impl::Awaiter>& awaiter, std::uintptr_t context) override {
        event_.waiters_->GetSignalOrAppend(awaiter, context);
    }

    boost::intrusive_ptr<impl::Awaiter> RemoveAwaiter(impl::Awaiter& awaiter, std::uintptr_t context)
        noexcept override {
        return event_.waiters_->Remove(awaiter, context);
    }

private:
    SingleConsumerEvent& event_;
};

SingleConsumerEvent::SingleConsumerEvent() noexcept = default;

SingleConsumerEvent::SingleConsumerEvent(NoAutoReset) noexcept : is_auto_reset_(false) {}

SingleConsumerEvent::~SingleConsumerEvent() = default;

bool SingleConsumerEvent::IsAutoReset() const noexcept { return is_auto_reset_; }

bool SingleConsumerEvent::WaitForEvent() { return WaitForEventUntil(Deadline{}); }

bool SingleConsumerEvent::WaitForEventUntil(Deadline deadline) {
    // optimistic path
    if (waiters_->IsSignaled()) {
        if (is_auto_reset_) {
            waiters_->GetAndResetSignal();
        }
        return true;
    }

    impl::TaskContext& current = current_task::GetCurrentTaskContext();
    EventAwaitable awaitable{*this};

    const auto wakeup_source = current.Sleep(awaitable, deadline);
    if (!impl::HasWaitSucceeded(wakeup_source)) {
        return false;
    }

    return GetIsSignaled();
}

void SingleConsumerEvent::Reset() noexcept { waiters_->GetAndResetSignal(); }

void SingleConsumerEvent::Send() { waiters_->SetSignalAndNotifyOne(); }

bool SingleConsumerEvent::IsReady() const noexcept { return waiters_->IsSignaled(); }

bool SingleConsumerEvent::GetIsSignaled() noexcept {
    if (is_auto_reset_) {
        return waiters_->GetAndResetSignal();
    } else {
        return waiters_->IsSignaled();
    }
}

void SingleConsumerEvent::CheckIsAutoResetForWaitPredicate() const {
    UINVARIANT(IsAutoReset(), "Wait with predicate requires auto-reset functionality");
}

}  // namespace engine

USERVER_NAMESPACE_END
