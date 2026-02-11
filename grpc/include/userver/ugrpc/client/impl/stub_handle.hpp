#pragma once

#include <userver/rcu/rcu.hpp>

#include <userver/ugrpc/impl/stub_any.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client::impl {

struct StubState;

class StubHandle final {
public:
    StubHandle(rcu::ReadablePtr<StubState>&& stub_state, ugrpc::impl::StubAny& stub);

    StubHandle(StubHandle&&) noexcept = default;
    StubHandle& operator=(StubHandle&&) = delete;

    StubHandle(const StubHandle&) = delete;
    StubHandle& operator=(const StubHandle&) = delete;

    ugrpc::impl::StubAny& Get() { return stub_; }

private:
    const rcu::ReadablePtr<StubState> stub_state_;
    ugrpc::impl::StubAny& stub_;
};

}  // namespace ugrpc::client::impl

USERVER_NAMESPACE_END
