#include <google/protobuf/json/json.h>
#include <google/protobuf/message.h>

#include <userver/proto-structs/json.hpp>
#include <userver/protobuf/string.hpp>

USERVER_NAMESPACE_BEGIN

namespace logging::impl {

logging::LogHelper& LogMessage(logging::LogHelper& h, const google::protobuf::Message& message) {
    auto options = google::protobuf::json::PrintOptions();
    options.always_print_primitive_fields = true;
    options.preserve_proto_field_names = true;

    protobuf::ProtoStringType out;
    const auto status = google::protobuf::json::MessageToJsonString(message, &out, options);
    if (status.ok()) {
        return h << out;
    }
    return h << "Failed to log struct: " << status;
}

}  // namespace logging::impl

USERVER_NAMESPACE_END
