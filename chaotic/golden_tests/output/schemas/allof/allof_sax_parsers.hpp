#pragma once

#include <userver/chaotic/object.hpp>
#include <userver/chaotic/primitive.hpp>
#include <userver/chaotic/sax_parser.hpp>
#include <userver/chaotic/validators.hpp>
#include <userver/chaotic/with_type.hpp>
#include <userver/formats/common/merge.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/utils/trivial_map.hpp>

#include "allof.hpp"
#include "allof_parsers.ipp"

namespace ns {

auto ParserOf(::ns::AllOf::Foo__P0&) {
  return USERVER_NAMESPACE::chaotic::sax::Parser<USERVER_NAMESPACE::chaotic::Object<
      ::ns::AllOf::Foo__P0, USERVER_NAMESPACE::chaotic::UnknownFields::StoreJson,
      USERVER_NAMESPACE::chaotic::Field<
          ::ns::AllOf::Foo__P0,
          USERVER_NAMESPACE::chaotic::Optional<USERVER_NAMESPACE::chaotic::Primitive<std::string>>,
          &::ns::AllOf::Foo__P0::foo, ::ns::AllOf::Foo__P0::kFieldNamefoo>>>{};
}

auto ParserOf(::ns::AllOf::Foo__P1&) {
  return USERVER_NAMESPACE::chaotic::sax::Parser<USERVER_NAMESPACE::chaotic::Object<
      ::ns::AllOf::Foo__P1, USERVER_NAMESPACE::chaotic::UnknownFields::StoreJson,
      USERVER_NAMESPACE::chaotic::Field<
          ::ns::AllOf::Foo__P1, USERVER_NAMESPACE::chaotic::Optional<USERVER_NAMESPACE::chaotic::Primitive<int>>,
          &::ns::AllOf::Foo__P1::bar, ::ns::AllOf::Foo__P1::kFieldNamebar>>>{};
}

auto ParserOf(::ns::AllOf&) {
  return USERVER_NAMESPACE::chaotic::sax::Parser<USERVER_NAMESPACE::chaotic::Object<
      ::ns::AllOf, USERVER_NAMESPACE::chaotic::UnknownFields::Forbid,
      USERVER_NAMESPACE::chaotic::Field<
          ::ns::AllOf, USERVER_NAMESPACE::chaotic::Optional<USERVER_NAMESPACE::chaotic::Primitive<::ns::AllOf::Foo>>,
          &::ns::AllOf::foo, ::ns::AllOf::kFieldNamefoo>>>{};
}

}  // namespace ns

