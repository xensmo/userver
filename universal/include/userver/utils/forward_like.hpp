#pragma once

/// @file userver/utils/forward_like.hpp
/// @brief @copybrief utils::ForwardLike

#include <type_traits>
#include <utility>

#include <userver/compiler/impl/nodebug.hpp>

USERVER_NAMESPACE_BEGIN

namespace utils {

// Analogue of std::forward_like from c++23.
template <typename TOwner, typename TMember>
USERVER_IMPL_NODEBUG decltype(auto) ForwardLike(TMember& member) {
    if constexpr (std::is_lvalue_reference_v<TOwner> || std::is_lvalue_reference_v<TMember>) {
        return member;
    } else {
        return std::move(member);
    }
}

// Analogue of std::forward_like from c++23.
template <typename TOwner, typename TMember>
USERVER_IMPL_NODEBUG decltype(auto) ForwardLike(const TMember& member) {
    return member;
}

}  // namespace utils

USERVER_NAMESPACE_END
