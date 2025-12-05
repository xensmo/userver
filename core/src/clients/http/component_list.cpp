#include <userver/clients/http/component_list.hpp>

#include <userver/clients/http/component.hpp>

USERVER_NAMESPACE_BEGIN

namespace clients::http {

components::ComponentList ComponentList() {
    return components::ComponentList().Append<components::HttpClientCore>().Append<components::HttpClient>();
}

}  // namespace clients::http

USERVER_NAMESPACE_END
