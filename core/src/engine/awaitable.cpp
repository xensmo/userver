#include <userver/engine/awaitable.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <userver/engine/impl/awaiter.hpp>
#include <userver/engine/impl/context_accessor.hpp>

USERVER_NAMESPACE_BEGIN

namespace engine {
namespace {

class ReadyAwaitable final : private impl::AwaitableBase {
public:
    AwaitableToken GetAwaitableToken() noexcept { return AwaitableToken{utils::impl::InternalTag{}, this}; }

private:
    bool IsReady() const noexcept override { return true; }

    void TryAppendAwaiter(boost::intrusive_ptr<impl::Awaiter>&, std::uintptr_t) override {}

    boost::intrusive_ptr<impl::Awaiter> RemoveAwaiter(impl::Awaiter&, std::uintptr_t) noexcept override {
        // The awaiter is always notified early by TryAppendAwaiter, nothing to remove.
        return {};
    }
};

constinit ReadyAwaitable kReadyAwaitable{};

}  // namespace

AwaitableToken MakeReadyAwaitableToken() noexcept { return kReadyAwaitable.GetAwaitableToken(); }

}  // namespace engine

USERVER_NAMESPACE_END
