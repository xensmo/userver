#pragma once
#include <string_view>

USERVER_NAMESPACE_BEGIN

namespace storages::redis {

enum class TopologyUpdateMethod { kClusterSlots, kClusterShards };

TopologyUpdateMethod ToTopologyUpdateMethod(std::string_view view);
std::string_view ToStringView(TopologyUpdateMethod);

}  // namespace storages::redis

USERVER_NAMESPACE_END
