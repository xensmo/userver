#include <ugrpc/client/impl/call_options_accessor.hpp>

#include <userver/ugrpc/time_utils.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client::impl {

std::unique_ptr<grpc::ClientContext> CallOptionsAccessor::CreateClientContext(const CallOptions& call_options) {
    auto client_context = call_options.client_context_factory_ ? call_options.client_context_factory_()
                                                               : std::make_unique<grpc::ClientContext>();

    // default timeout_ gives unreachable deadline
    // engine::Deadline::FromDuration(std::chrono::milliseconds::max()) -> !IsReachable
    const auto deadline = std::min(call_options.deadline_, engine::Deadline::FromDuration(call_options.timeout_));
    if (deadline.IsReachable()) {
        client_context->set_deadline(deadline);
    }

    for (const auto& [meta_key, meta_value] : call_options.metadata_) {
        client_context->AddMetadata(meta_key, meta_value);
    }

    return client_context;
}

}  // namespace ugrpc::client::impl

USERVER_NAMESPACE_END
