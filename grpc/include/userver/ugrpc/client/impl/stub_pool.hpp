#pragma once

#include <grpcpp/channel.h>

#include <userver/utils/fixed_array.hpp>

#include <userver/ugrpc/impl/stub_any.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client::impl {

class StubPool final {
public:
    StubPool() = default;

    StubPool(
        utils::FixedArray<std::shared_ptr<grpc::Channel>>&& channels,
        utils::FixedArray<ugrpc::impl::StubAny>&& stubs
    );

    std::size_t Size() const { return stubs_.size(); }

    ugrpc::impl::StubAny& NextStub() const;

    const utils::FixedArray<std::shared_ptr<grpc::Channel>>& GetChannels() const { return channels_; }

private:
    utils::FixedArray<std::shared_ptr<grpc::Channel>> channels_;
    mutable utils::FixedArray<ugrpc::impl::StubAny> stubs_;
};

}  // namespace ugrpc::client::impl

USERVER_NAMESPACE_END
