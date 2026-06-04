#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include <engine/task/task_context.hpp>
#include <userver/engine/impl/context_accessor.hpp>
#include <userver/utils/fast_scope_guard.hpp>
#include <userver/utils/span.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

class WaitAnyAwaitable final : public WeakAwaitable {
public:
    explicit WaitAnyAwaitable(utils::span<ContextAccessor*> targets)
        : targets_(targets)
    {}

    bool IsReady() const noexcept override {
        return std::ranges::all_of(targets_, [](ContextAccessor* target) {
            return target == nullptr || target->IsReady();
        });
    }

    void TryAppendAwaiter(boost::intrusive_ptr<Awaiter>& awaiter, std::uintptr_t context) override {
        for (auto*& target : targets_) {
            if (!target) {
                continue;
            }

            utils::FastScopeGuard disable_wakeups([&]() noexcept {
                DoDisableWakeups(utils::span{targets_.data(), &target}, *awaiter, context);
            });

            // TryAppendAwaiter might throw.
            auto awaiter_copy = awaiter;
            target->TryAppendAwaiter(awaiter_copy, context);
            if (awaiter_copy != nullptr) {  // target is ready.
                return;
            }

            disable_wakeups.Release();
        }

        // Mark that the compound awaitable is not ready. A possible optimization (not performed currently)
        // is to pass `awaiter` to the last target directly instead of copying it.
        awaiter = nullptr;
    }

    boost::intrusive_ptr<Awaiter> RemoveAwaiter(Awaiter& awaiter, std::uintptr_t context) noexcept override {
        return DoDisableWakeups(targets_, awaiter, context);
    }

private:
    boost::intrusive_ptr<Awaiter> DoDisableWakeups(
        utils::span<ContextAccessor*> targets,
        Awaiter& awaiter,
        std::uintptr_t context
    ) const noexcept {
        boost::intrusive_ptr<Awaiter> compound_removal_result;
        bool removed_before_notification = true;

        for (auto* const target : targets) {
            if (!target) {
                continue;
            }

            auto removal_result = target->RemoveAwaiter(awaiter, context);
            removed_before_notification &= (removal_result != nullptr);
            compound_removal_result = std::move(removal_result);
        }

        if (!removed_before_notification) {
            compound_removal_result = nullptr;
        }
        return compound_removal_result;
    }

    const utils::span<ContextAccessor*> targets_;
};

inline bool AreUniqueValues(utils::span<ContextAccessor*> targets) {
    std::vector<ContextAccessor*> sorted;
    sorted.reserve(targets.size());
    std::ranges::copy_if(targets, std::back_inserter(sorted), [](const auto& target) { return target != nullptr; });
    std::ranges::sort(sorted);
    return std::ranges::adjacent_find(sorted) == sorted.end();
}

}  // namespace engine::impl

USERVER_NAMESPACE_END
