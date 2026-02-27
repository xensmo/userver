#include <userver/ugrpc/client/completion_status.hpp>

USERVER_NAMESPACE_BEGIN

namespace ugrpc::client {

std::string_view ToString(SpecialCaseCompletionType type) {
    switch (type) {
        case SpecialCaseCompletionType::kNetworkError:
            return "network_error";
        case SpecialCaseCompletionType::kTimeoutDeadlinePropagated:
            return "timeout_deadline_propagated";
        case SpecialCaseCompletionType::kCancelled:
            return "cancelled";
        case SpecialCaseCompletionType::kAbandoned:
            return "abandoned";
    }
    return "unknown";
}

}  // namespace ugrpc::client

USERVER_NAMESPACE_END
