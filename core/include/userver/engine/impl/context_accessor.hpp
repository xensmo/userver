#pragma once

#include <cstdint>
#include <exception>

#include <boost/smart_ptr/intrusive_ptr.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine::impl {

class Awaiter;
class TaskContext;

void intrusive_ptr_add_ref(Awaiter* awaiter) noexcept;  // NOLINT(readability-identifier-naming)
void intrusive_ptr_release(Awaiter* awaiter) noexcept;  // NOLINT(readability-identifier-naming)

class WeakAwaitable {
public:
    // Auxiliary method to check if the awaitable is ready.
    // A conforming implementation may return `false` if it's not sure about the awaitable's state.
    virtual bool IsReady() const noexcept = 0;

    // Atomically:
    //
    // 1. If not `IsReady`, then
    //    * move from `awaiter`;
    //    * store `awaiter` and `context` somewhere to notify when `IsReady() == true` is reached.
    // 2. If `IsReady`, then
    //    * do not move from `awaiter`;
    //    * do not notify `awaiter`.
    //
    // You may not sleep in `TryAppendAwaiter`.
    virtual void TryAppendAwaiter(boost::intrusive_ptr<Awaiter>& awaiter, std::uintptr_t context) = 0;

    // Atomically:
    //
    // 1. If `awaiter` was not notified, then
    //    * remove `awaiter` from the internal wait list;
    //    * return the non-null owning pointer to `awaiter` that was stored previously.
    // 2. If `awaiter` was already notified (or is in the process of being notified), then
    //    * an owning pointer to `awaiter` is already extracted and was passed or will be passed to `Notify`;
    //    * return `nullptr`;
    //    * the notification is guaranteed to arrive eventually.
    //
    // Either way, `RemoveAwaiter` should allow to call `TryAppendAwaiter` again safely with the same
    // or a different `awaiter`.
    //
    // Depending on a wait list implementation `context` match also could be required for the awaiter removal.
    // You may not sleep in `RemoveAwaiter`.
    virtual boost::intrusive_ptr<Awaiter> RemoveAwaiter(Awaiter& awaiter, std::uintptr_t context) noexcept = 0;

    // Returns the stored exception if present.
    // Precondition: IsReady.
    // This method is required for WaitAllChecked to properly function.
    virtual std::exception_ptr GetErrorResult() const noexcept { return {}; }

protected:
    WeakAwaitable();

    ~WeakAwaitable() = default;
};

// Provides additional guarantees over WeakAwaitable:
//
// * No spurious wakeups are allowed: upon calling `Awaiter::Notify`, the awaiting process is complete;
//   no resources can be stolen, requiring retries;
// * Cancellations are supported: `RemoveAwaiter` can be called at any time (not only upon completion), and it works;
// * Each `Awaiter` will only be notified no more than once, and no duplication of `intrusive_ptr` is allowed.
//
// This makes it usable with WaitAny.
//
// TODO rename to Awaitable.
class ContextAccessor : public WeakAwaitable {
protected:
    ContextAccessor();

    ~ContextAccessor() = default;
};

}  // namespace engine::impl

USERVER_NAMESPACE_END
