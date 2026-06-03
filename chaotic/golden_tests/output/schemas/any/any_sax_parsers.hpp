#pragma once

#include <userver/chaotic/additional_properties.hpp>
#include <userver/chaotic/primitive.hpp>
#include <userver/chaotic/sax_parser.hpp>
#include <userver/chaotic/with_type.hpp>
#include <userver/formats/parse/common_containers.hpp>
#include <userver/formats/serialize/common_containers.hpp>
#include <userver/utils/trivial_map.hpp>

#include "any.hpp"

namespace ns {

[[maybe_unused]] USERVER_NAMESPACE::chaotic::sax::Parser<USERVER_NAMESPACE::chaotic::Object<
    ::ns::WithAnyField, USERVER_NAMESPACE::chaotic::UnknownFields::Forbid,
    USERVER_NAMESPACE::chaotic::Field<::ns::WithAnyField,
                                      USERVER_NAMESPACE::chaotic::Optional<USERVER_NAMESPACE::formats::json::Value>,
                                      &::ns::WithAnyField::payload, ::ns::WithAnyField::kFieldNamepayload>>>
    ParserOf(USERVER_NAMESPACE::chaotic::sax::Type<WithAnyField>);

}  // namespace ns

