#pragma once

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

enum class [[nodiscard]] EarlyNotify : bool {};

class Awaiter;

class ContextAccessor {
public:
    virtual bool IsReady() const noexcept = 0;

    // Atomically:
    // 1. if not `IsReady`, then store `awaiter` somewhere to notify when
    //    `IsReady() == true` is reached, and return `EarlyNotify{false}`;
    // 2. if `IsReady`, then return `EarlyNotify{true}`. Awaiter is not notified.
    virtual EarlyNotify TryAppendAwaiter(Awaiter& awaiter) = 0;

    // Remove `awaiter` from the internal wait list if it's still there.
    // You may not sleep in `RemoveAwaiter`, unlike in `AfterWait`.
    virtual void RemoveAwaiter(Awaiter& awaiter) noexcept = 0;

    // Wait for some cleanup (e.g. wait for `awaiter` to actually remove itself).
    // You may sleep in `AfterWait`.
    virtual void AfterWait() noexcept = 0;

    // Precondition: IsReady
    // This method is required for WaitAllChecked to properly function.
    virtual void RethrowErrorResult() const = 0;

protected:
    ContextAccessor();

    ~ContextAccessor() = default;
};

}  // namespace engine::impl

USERVER_NAMESPACE_END
