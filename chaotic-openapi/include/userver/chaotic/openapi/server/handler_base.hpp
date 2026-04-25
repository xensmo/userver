#pragma once

/// @file userver/chaotic/openapi/server/handler_base.hpp
/// @brief Base class template for chaotic-generated HTTP handlers

#include <string_view>
#include <type_traits>

#include <userver/chaotic/openapi/server/dependencies.hpp>
#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/components/container.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/request/request_context.hpp>

USERVER_NAMESPACE_BEGIN

namespace chaotic::openapi::server {

/// @brief Base class for generated HTTP handlers.
///
/// @tparam kName      Handler name, must be a `constexpr std::string_view`
///                    with static storage duration.
/// @tparam ErrorResponseBase  Base class of all throwable error responses for this
///                    operation, or `void` if there are none.
/// @tparam Request    Parsed request type for this operation.
/// @tparam Response   Response variant type for this operation.
/// @tparam HandlerTag Unique tag type for dependency injection keying.
///
/// The generated `Handler` subclass implements `Handle()`.
template <
    const std::string_view& Name,
    typename ErrorResponseBase,
    typename Request,
    typename Response,
    typename HandlerTag>
class BaseHandler : public USERVER_NAMESPACE::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = Name;

    using Deps = chaotic::openapi::server::dependencies::ForHandler<HandlerTag>;

    BaseHandler(
        const USERVER_NAMESPACE::components::ComponentConfig& config,
        const USERVER_NAMESPACE::components::ComponentContext& context
    )
        : USERVER_NAMESPACE::server::handlers::HttpHandlerBase(config, context),
          factories_(context.FindComponent<FactoriesContainer>())
    {}

    ~BaseHandler() override = default;

    // TODO: shift to View::Handle(...)
    virtual Response Handle(Request& request, Deps& deps) const = 0;

private:
    using Factories = chaotic::openapi::server::dependencies::Factories;
    using FactoriesContainer = USERVER_NAMESPACE::components::Container<Factories>;
    using HttpRequest = USERVER_NAMESPACE::server::http::HttpRequest;

    static Request ParseRequestOrThrow(const HttpRequest& http_request) {
        try {
            return ParseRequest<Request>(http_request);
        } catch (const USERVER_NAMESPACE::server::handlers::ClientError&) {
            throw;
        } catch (const std::exception& e) {
            throw USERVER_NAMESPACE::server::handlers::ClientError(USERVER_NAMESPACE::server::handlers::ExternalBody{
                e.what()
            });
        }
    }

    std::string DoHandle(Request& request, HttpRequest& http_request) const {
        auto deps = factories_.Get().template Make<HandlerTag>();
        auto response = Handle(request, deps);
        return SerializeResponse(std::move(response), http_request);
    }

    template <typename T>
    std::string HandleParsed(Request& request, HttpRequest& http_request) const {
        try {
            return DoHandle(request, http_request);
        } catch (ErrorResponseBase& e) {
            return SerializeResponse(std::move(e), http_request);
        }
    }

    template <>
    std::string HandleParsed<void>(Request& request, HttpRequest& http_request) const {
        return DoHandle(request, http_request);
    }

    std::string HandleRequest(HttpRequest& http_request, USERVER_NAMESPACE::server::request::RequestContext&)
        const final {
        auto request = ParseRequestOrThrow(http_request);
        HandleParsed<Request>(request, http_request);
    }

    FactoriesContainer& factories_;
};

}  // namespace chaotic::openapi::server

USERVER_NAMESPACE_END
