#include <userver/engine/wait_any.hpp>

#include <deque>

#include <engine/impl/wait_any_utils.hpp>
#include <engine/task/task_context.hpp>
#include <userver/concurrent/impl/intrusive_mpsc_queue.hpp>
#include <userver/engine/impl/awaiter.hpp>
#include <userver/engine/single_consumer_event.hpp>
#include <userver/utils/enumerate.hpp>
#include <userver/utils/make_intrusive_ptr.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine {

namespace impl {
std::optional<std::size_t> DoWaitAny(utils::span<ContextAccessor*> targets, Deadline deadline) {
    UASSERT_MSG(AreUniqueValues(targets), "Same tasks/futures were detected in WaitAny* call");
    bool none_valid = true;

    for (const auto& [idx, target] : utils::enumerate(targets)) {
        if (!target) {
            continue;
        }
        none_valid = false;
        if (target->IsReady()) {
            return idx;
        }
    }

    if (none_valid) {
        return std::nullopt;
    }

    auto& current = current_task::GetCurrentTaskContext();
    WaitAnyWaitStrategy wait_strategy{targets, current};
    current.Sleep(wait_strategy, deadline);

    for (const auto& [idx, target] : utils::enumerate(targets)) {
        if (target && target->IsReady()) {
            return idx;
        }
    }

    return std::nullopt;
}

}  // namespace impl

class WaitAnyContext::Impl final : public impl::PolymorphicAwaiter {
public:
    Impl();
    ~Impl() noexcept = default;

    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void Append(impl::ContextAccessor* awaitable);

    std::optional<std::size_t> WaitUntil(Deadline deadline);

    std::size_t GetCount() const noexcept { return context_accessors_.size(); }

    void Reserve(std::size_t count);

    void Unsubscribe() noexcept;

private:
    struct QueueItem : concurrent::impl::SinglyLinkedBaseHook {
        explicit QueueItem(std::size_t index)
            : index(index)
        {}

        std::size_t index;
        bool subscribed{false};
    };

    using Queue = concurrent::impl::IntrusiveMpscQueue<QueueItem>;

    void DoNotify(std::uintptr_t context) noexcept override;

    void Destroy() noexcept override { delete this; }

    std::optional<std::size_t> TryProcessQueue() noexcept;

    std::optional<std::size_t> TrySubscribe();

    bool ProcessNotified(QueueItem& item) noexcept;

    std::vector<impl::ContextAccessor*> context_accessors_;
    std::size_t subscribed_awaiters_count_{0};
    std::deque<QueueItem> queue_items_;
    Queue notified_;
    engine::SingleConsumerEvent queue_non_empty_{engine::SingleConsumerEvent::NoAutoReset{}};
};

WaitAnyContext::Impl::Impl()
    : PolymorphicAwaiter(Awaiter::InitialRefCounter::kOne)
{}

void WaitAnyContext::Impl::Append(impl::ContextAccessor* awaitable) { context_accessors_.push_back(awaitable); }

std::optional<std::size_t> WaitAnyContext::Impl::WaitUntil(Deadline deadline) {
    for (;;) {
        queue_non_empty_.Reset();
        if (subscribed_awaiters_count_ > 0) {
            auto result = TryProcessQueue();
            if (result.has_value()) {
                return result;
            }
        }

        auto result = TrySubscribe();
        if (result.has_value()) {
            return result;
        }

        if (subscribed_awaiters_count_ == 0) {
            return std::nullopt;
        }

        if (!queue_non_empty_.WaitForEventUntil(deadline)) {
            return std::nullopt;
        }
    }
}

void WaitAnyContext::Impl::Reserve(std::size_t count) { context_accessors_.reserve(count); }

void WaitAnyContext::Impl::Unsubscribe() noexcept {
    for (std::size_t i = 0; i < queue_items_.size() && subscribed_awaiters_count_ > 0; ++i) {
        auto context_accessor = context_accessors_[i];
        if (context_accessor == nullptr || !queue_items_[i].subscribed) {
            continue;
        }
        context_accessor->RemoveAwaiter(*this, reinterpret_cast<std::uintptr_t>(&queue_items_[i]));
        --subscribed_awaiters_count_;
    }
}

void WaitAnyContext::Impl::DoNotify(std::uintptr_t context) noexcept {
    notified_.Push(*reinterpret_cast<QueueItem*>(context));
    queue_non_empty_.Send();
}

std::optional<std::size_t> WaitAnyContext::Impl::TryProcessQueue() noexcept {
    for (;;) {
        QueueItem* const item = notified_.TryPopBlocking();
        if (item == nullptr) {
            return std::nullopt;
        }

        if (ProcessNotified(*item)) {
            return item->index;
        }
    }
}

std::optional<std::size_t> WaitAnyContext::Impl::TrySubscribe() {
    while (queue_items_.size() < context_accessors_.size()) {
        const std::size_t index = queue_items_.size();
        auto& item = queue_items_.emplace_back(index);
        if (context_accessors_[index] == nullptr) {
            continue;
        }

        if (context_accessors_[index]->TryAppendAwaiter(*this, reinterpret_cast<std::uintptr_t>(&item)) ==
            impl::EarlyNotify{true})
        {
            return index;
        }

        item.subscribed = true;
        ++subscribed_awaiters_count_;
    }
    return std::nullopt;
}

bool WaitAnyContext::Impl::ProcessNotified(QueueItem& item) noexcept {
    UASSERT(subscribed_awaiters_count_ > 0);
    UASSERT(item.index < context_accessors_.size());
    UASSERT(context_accessors_[item.index]);

    // The caller might have already changed the awaitable's state between Wait* calls.
    // So, we check the ready flag to avoid erroneous wakeups on non-ready awaitables.
    const bool result = context_accessors_[item.index]->IsReady();
    context_accessors_[item.index] = nullptr;
    item.subscribed = false;
    --subscribed_awaiters_count_;
    return result;
}

WaitAnyContext::WaitAnyContext()
    : impl_(new WaitAnyContext::Impl(), /*add_ref=*/false)
{}

WaitAnyContext::~WaitAnyContext() noexcept {
    if (impl_ == nullptr) {
        // Instance has been moved out.
        return;
    }
    impl_->Unsubscribe();
}

WaitAnyContext::WaitAnyContext(WaitAnyContext&&) noexcept = default;

WaitAnyContext& WaitAnyContext::operator=(WaitAnyContext&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    WaitAnyContext cleanup{std::move(*this)};
    impl_ = std::move(other.impl_);
    return *this;
}

std::optional<std::size_t> WaitAnyContext::Wait() { return WaitUntil(Deadline{}); }

std::optional<std::size_t> WaitAnyContext::WaitUntil(Deadline deadline) {
    UASSERT(impl_ != nullptr);
    return impl_->WaitUntil(deadline);
}

std::size_t WaitAnyContext::GetCount() const noexcept {
    UASSERT(impl_ != nullptr);
    return impl_->GetCount();
}

void WaitAnyContext::Reserve(std::size_t count) {
    UASSERT(impl_ != nullptr);
    impl_->Reserve(count);
}

void WaitAnyContext::AppendAccessor(impl::ContextAccessor* awaitable) {
    UASSERT(impl_ != nullptr);
    impl_->Append(awaitable);
}

}  // namespace engine

USERVER_NAMESPACE_END
