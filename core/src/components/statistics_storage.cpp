#include <userver/components/statistics_storage.hpp>

#include <userver/yaml_config/merge_schemas.hpp>

#ifndef ARCADIA_ROOT
#include "generated/src/components/statistics_storage.yaml.hpp"  // Y_IGNORE
#endif

USERVER_NAMESPACE_BEGIN

namespace components {

StatisticsStorage::StatisticsStorage(const ComponentConfig&, const ComponentContext&)
    : metrics_storage_(std::make_shared<utils::statistics::MetricsStorage>()),
      metrics_storage_registration_(metrics_storage_->RegisterIn(storage_))
{}

StatisticsStorage::~StatisticsStorage() {
    for (auto& entry : metrics_storage_registration_) {
        entry.Unregister();
    }
}

void StatisticsStorage::OnAllComponentsLoaded() { storage_.StopRegisteringExtenders(); }

yaml_config::Schema StatisticsStorage::GetStaticConfigSchema() {
    return yaml_config::MergeSchemasFromResource<RawComponentBase>("src/components/statistics_storage.yaml");
}

}  // namespace components

USERVER_NAMESPACE_END
