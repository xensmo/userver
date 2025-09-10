"""`TypeReference` constants for common built-in C++ types."""

from proto_structs.models import includes
from proto_structs.models import type_ref

OPTIONAL_TEMPLATE = type_ref.BuiltinType(full_cpp_name='std::optional', include=includes.BUNDLE_STRUCTS_HPP)
VECTOR_TEMPLATE = type_ref.BuiltinType(full_cpp_name='std::vector', include=includes.BUNDLE_STRUCTS_HPP)
HASH_MAP_TEMPLATE = type_ref.UserverLibraryType(
    full_cpp_name_wo_userver='proto_structs::HashMap', include='userver/proto-structs/hash_map.hpp'
)

BOX_TEMPLATE = type_ref.UserverLibraryType(full_cpp_name_wo_userver='utils::Box', include='userver/utils/box.hpp')

ONEOF_BASE_CLASS = type_ref.UserverLibraryType(
    full_cpp_name_wo_userver='proto_structs::Oneof', include='userver/proto-structs/impl/oneof_codegen.hpp'
)


def make_optional(value: type_ref.TypeReference) -> type_ref.TypeReference:
    return type_ref.TemplateType(template=OPTIONAL_TEMPLATE, template_args=(value,))


def make_vector(value: type_ref.TypeReference) -> type_ref.TypeReference:
    return type_ref.TemplateType(
        template=VECTOR_TEMPLATE,
        template_args=(value,),
        works_with_forward_references=True,
    )


def make_hash_map(key: type_ref.TypeReference, value: type_ref.TypeReference) -> type_ref.TypeReference:
    return type_ref.TemplateType(
        template=HASH_MAP_TEMPLATE,
        template_args=(
            key,
            value,
        ),
    )


def make_box(value: type_ref.TypeReference) -> type_ref.TypeReference:
    return type_ref.TemplateType(template=BOX_TEMPLATE, template_args=(value,), works_with_forward_references=True)
