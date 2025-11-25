#include <userver/clients/http/plugins/retry_budget/component.hpp>

#include <clients/http/plugins/retry_budget/plugin.hpp>

#include <userver/components/component_config.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#ifndef ARCADIA_ROOT
#include "generated/src/clients/http/plugins/retry_budget/component.yaml.hpp"  // Y_IGNORE
#endif

USERVER_NAMESPACE_BEGIN

namespace clients::http::plugins::retry_budget {

Component::Component(const components::ComponentConfig& config, const components::ComponentContext& context)
    : ComponentBase(config, context),
      plugin_(std::make_unique<retry_budget::Plugin>())
{
    auto policy = config.As<USERVER_NAMESPACE::utils::RetryBudgetSettings>();
    plugin_->Storage().SetPolicy(policy);
}

Component::~Component() = default;

http::Plugin& Component::GetPlugin() { return *plugin_; }

yaml_config::Schema Component::GetStaticConfigSchema() {
    return yaml_config::MergeSchemasFromResource<
        components::ComponentBase>("src/clients/http/plugins/retry_budget/component.yaml");
}

}  // namespace clients::http::plugins::retry_budget

USERVER_NAMESPACE_END
