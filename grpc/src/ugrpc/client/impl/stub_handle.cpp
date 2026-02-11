#include <userver/ugrpc/client/impl/stub_handle.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client::impl {

StubHandle::StubHandle(rcu::ReadablePtr<StubState>&& stub_state, ugrpc::impl::StubAny& stub)
    : stub_state_{std::move(stub_state)},
      stub_{stub}
{}

}  // namespace ugrpc::client::impl

USERVER_NAMESPACE_END
