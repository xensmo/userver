#pragma once

#include <userver/chaotic/const_value.hpp>
#include <userver/chaotic/type_bundle_hpp.hpp>

#include "const_fwd.hpp"

namespace ns {

inline constexpr int kTheBestValueConst = 42;

using TheBestValue = USERVER_NAMESPACE::chaotic::ConstValue<kTheBestValueConst>;

}  // namespace ns

