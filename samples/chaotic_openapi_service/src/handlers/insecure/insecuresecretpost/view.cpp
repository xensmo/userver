#include "view.hpp"

#include <fmt/format.h>

#include <handlers/insecure/insecuresecretpost/handler.hpp>

namespace handlers::insecure::insecuresecretpost {

Response View::Handle(Request&& request, Deps&& /*deps*/) {
    return Response200{.body = {.greeting = fmt::format("Hello, {}!", request.name)}};
}

std::string View::GetResponseForLogging(
    const Response& /*response*/,
    const std::string& /*serialized_response*/,
    USERVER_NAMESPACE::server::request::RequestContext& /*context*/
) {
    return {};
}

}  // namespace handlers::insecure::insecuresecretpost
