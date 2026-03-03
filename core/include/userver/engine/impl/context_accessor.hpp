#pragma once

#include <cstdint>
#include <exception>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

enum class [[nodiscard]] EarlyNotify : bool {};

class Awaiter;
class TaskContext;

class ContextAccessor {
public:
    virtual bool IsReady() const noexcept = 0;

    // Atomically:
    // 1. if not `IsReady`, then store `awaiter` and `context` somewhere to notify when
    //    `IsReady() == true` is reached, and return `EarlyNotify{false}`;
    // 2. if `IsReady`, then return `EarlyNotify{true}`. Awaiter is not notified.
    // You may not sleep in `TryAppendAwaiter`.
    virtual EarlyNotify TryAppendAwaiter(Awaiter& awaiter, std::uintptr_t context) = 0;

    // Remove `awaiter` from the internal wait list if it's still there.
    // Depending on a wait list implementation `context` match also could be required for the awaiter removal.
    // You may not sleep in `RemoveAwaiter`.
    virtual void RemoveAwaiter(Awaiter& awaiter, std::uintptr_t context) noexcept = 0;

    // Returns the stored exception if present.
    // Precondition: IsReady.
    // This method is required for WaitAllChecked to properly function.
    virtual std::exception_ptr GetErrorResult() const noexcept { return {}; }

protected:
    ContextAccessor();

    ~ContextAccessor() = default;
};

}  // namespace engine::impl

USERVER_NAMESPACE_END
