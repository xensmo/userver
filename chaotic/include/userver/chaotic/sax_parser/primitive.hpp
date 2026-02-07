#pragma once

#include <userver/chaotic/sax_parser_join.hpp>
#include <userver/formats/json/parser/array_parser.hpp>
#include <userver/formats/json/parser/bool_parser.hpp>
#include <userver/formats/json/parser/int_parser.hpp>
#include <userver/formats/json/parser/number_parser.hpp>
#include <userver/formats/json/parser/string_parser.hpp>
#include <userver/utils/meta.hpp>

USERVER_NAMESPACE_BEGIN

namespace chaotic::sax {

inline auto ParserOf(std::int32_t&)
{
    return formats::json::parser::Int32Parser{};
}

inline auto ParserOf(std::int64_t&)
{
    return formats::json::parser::Int64Parser{};
}

inline auto ParserOf(bool&)
{
    return formats::json::parser::BoolParser{};
}

inline auto ParserOf(float&)
{
    return formats::json::parser::FloatParser{};
}

inline auto ParserOf(double&)
{
    return formats::json::parser::DoubleParser{};
}

inline auto ParserOf(std::string&)
{
    return formats::json::parser::StringParser{};
}

template <typename Array, typename = std::enable_if_t<meta::kIsRange<Array> && !meta::kIsMap<Array>>>
auto ParserOf(Array&)
{
    using Value = typename Array::value_type;
    using ItemParser = decltype(ParserOf(std::declval<Value&>()));
    using Parser = formats::json::parser::ArrayParser<Value, ItemParser, Array>;
    return JoinedParser<Parser, ItemParser>{};
}

template <typename T, typename Value = decltype(ParserOf(std::declval<T&>()))>
using Parser = Value;

}  // namespace chaotic::sax

USERVER_NAMESPACE_END
