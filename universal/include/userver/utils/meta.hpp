#pragma once

/// @file userver/utils/meta.hpp
/// @brief Metaprogramming, template variables and concepts
/// @ingroup userver_universal

#include <concepts>
#include <iosfwd>
#include <iterator>
#include <optional>
#include <type_traits>
#include <vector>

#include <userver/compiler/impl/nodebug.hpp>
#include <userver/utils/meta_light.hpp>

USERVER_NAMESPACE_BEGIN

namespace meta {

namespace impl {

using std::begin;
using std::end;

template <typename T>
concept HasKeyType = requires { typename T::key_type; };

template <typename T>
concept HasMappedType = requires { typename T::mapped_type; };

template <typename T>
concept IsRange = requires(T& t) {
    requires std::is_same_v<std::decay_t<decltype(begin(t))>, std::decay_t<decltype(end(t))>>;
};

template <IsRange T>
using IteratorType USERVER_IMPL_NODEBUG = decltype(begin(std::declval<T&>()));

template <typename T>
using RangeValueType USERVER_IMPL_NODEBUG = typename std::iterator_traits<IteratorType<T>>::value_type;

template <typename T>
struct IsFixedSizeContainer : std::false_type {};

// Boost and std arrays
template <typename T, std::size_t Size, template <typename, std::size_t> typename Array>
struct IsFixedSizeContainer<Array<T, Size>> : std::bool_constant<sizeof(Array<T, Size>) == sizeof(T) * Size> {};

template <typename... Args>
concept IsSingleRange = (sizeof...(Args) == 1) && (impl::IsRange<Args> && ...);

}  // namespace impl

template <typename T>
concept IsVector = kIsInstantiationOf<std::vector, T>;

template <typename T>
concept IsRange = impl::IsRange<T>;

/// Returns true if T is an ordered or unordered map or multimap
template <typename T>
concept IsMap = IsRange<T> && requires {
    typename T::key_type;
    typename T::mapped_type;
};

/// Returns true if T is a map (but not a multimap!)
template <typename T>
concept IsUniqueMap = IsMap<T> && requires(T& map, typename T::key_type key) {
    map[key];  // no operator[] in multimaps
};

template <impl::HasKeyType T>
using MapKeyType = typename T::key_type;

template <impl::HasMappedType T>
using MapValueType = typename T::mapped_type;

template <impl::IsRange T>
using RangeValueType = impl::RangeValueType<T>;

template <typename T>
concept IsRecursiveRange = IsRange<T> && std::same_as<impl::RangeValueType<T>, T>;

template <typename T>
concept IsIterator = requires { typename std::iterator_traits<T>::iterator_category; };

template <typename T>
concept IsOptional = kIsInstantiationOf<std::optional, T>;

template <typename T>
concept IsOstreamWritable = requires(std::ostream& os, const std::remove_reference_t<T>& val) {
    {
        os << val
    } -> std::same_as<std::ostream&>;
};

template <typename T>
concept IsStdHashable = requires(const T& val) {
    {
        std::hash<T>{}(val)
    } -> std::same_as<std::size_t>;
} && std::equality_comparable<T>;

/// @brief  Check if std::size is applicable to container
template <typename T>
concept IsSizable = requires(T value) { std::size(value); };

/// @brief Check if a container has `reserve`
template <typename T>
concept IsReservable = requires(T value) { value.reserve(1); };

/// @brief Check if a container has 'push_back'
template <typename T>
concept IsPushBackable = requires(T value) { value.push_back({}); };

/// @brief Check if a container has fixed size (e.g. std::array)
template <typename T>
concept IsFixedSizeContainer = impl::IsFixedSizeContainer<T>::value;

/// @brief Returns default inserter for a container
template <typename T>
auto Inserter(T& container) {
    if constexpr (IsPushBackable<T>) {
        return std::back_inserter(container);
    } else if constexpr (IsFixedSizeContainer<T>) {
        return container.begin();
    } else {
        return std::inserter(container, container.end());
    }
}

/// @deprecated Use @ref meta::IsVector instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsVector = IsVector<T>;

/// @deprecated Use @ref meta::IsRange instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsRange = IsRange<T>;

/// @deprecated Use @ref meta::IsMap instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsMap = IsMap<T>;

/// @deprecated Use @ref meta::IsOptional instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsOptional = IsOptional<T>;

/// @deprecated Use @ref meta::IsSizable instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsSizable = IsSizable<T>;

/// @deprecated Use @ref meta::IsReservable instead.
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
concept kIsReservable = IsReservable<T>;

}  // namespace meta

USERVER_NAMESPACE_END
