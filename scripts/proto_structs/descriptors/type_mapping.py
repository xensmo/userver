"""Parsers from Protobuf descriptors to TypeReference."""

# Documentation on Protobuf descriptors:
# https://googleapis.dev/python/protobuf/latest/google/protobuf/descriptor.html

from collections.abc import Sequence
import pathlib
import typing
from typing import List
from typing import Mapping
from typing import Optional
from typing import Union

import google.protobuf.descriptor as descriptor

from proto_structs.descriptors import descriptor_proto
from proto_structs.models import names
from proto_structs.models import type_ref
from proto_structs.models import type_ref_consts

TypeDescriptor = Union[descriptor.Descriptor, descriptor.EnumDescriptor]


BUILTIN_TYPES: Mapping[int, type_ref.TypeReference] = {
    descriptor.FieldDescriptor.TYPE_BOOL: type_ref.KeywordType(full_cpp_name='bool'),
    descriptor.FieldDescriptor.TYPE_FLOAT: type_ref.KeywordType(full_cpp_name='float'),
    descriptor.FieldDescriptor.TYPE_DOUBLE: type_ref.KeywordType(full_cpp_name='double'),
    descriptor.FieldDescriptor.TYPE_STRING: type_ref.BuiltinType(full_cpp_name='std::string'),
    descriptor.FieldDescriptor.TYPE_BYTES: type_ref.BuiltinType(full_cpp_name='std::string'),
    descriptor.FieldDescriptor.TYPE_INT32: type_ref.BuiltinType(full_cpp_name='std::int32_t'),
    descriptor.FieldDescriptor.TYPE_INT64: type_ref.BuiltinType(full_cpp_name='std::int64_t'),
    descriptor.FieldDescriptor.TYPE_UINT32: type_ref.BuiltinType(full_cpp_name='std::uint32_t'),
    descriptor.FieldDescriptor.TYPE_UINT64: type_ref.BuiltinType(full_cpp_name='std::uint64_t'),
    descriptor.FieldDescriptor.TYPE_SINT32: type_ref.BuiltinType(full_cpp_name='std::int32_t'),
    descriptor.FieldDescriptor.TYPE_SINT64: type_ref.BuiltinType(full_cpp_name='std::int64_t'),
    descriptor.FieldDescriptor.TYPE_FIXED32: type_ref.BuiltinType(full_cpp_name='std::uint32_t'),
    descriptor.FieldDescriptor.TYPE_FIXED64: type_ref.BuiltinType(full_cpp_name='std::uint64_t'),
    descriptor.FieldDescriptor.TYPE_SFIXED32: type_ref.BuiltinType(full_cpp_name='std::int32_t'),
    descriptor.FieldDescriptor.TYPE_SFIXED64: type_ref.BuiltinType(full_cpp_name='std::int64_t'),
}


def parse_enum_reference(field_type: descriptor.EnumDescriptor) -> type_ref.TypeReference:
    return type_ref.UserverCodegenType(
        name=names.make_structs_type_name(vanilla_type_name=parse_type_name(field_type)),
        proto_file=parse_file_path(field_type),
    )


def parse_struct_reference(field_type: descriptor.Descriptor) -> type_ref.TypeReference:
    return type_ref.UserverCodegenType(
        name=names.make_structs_type_name(vanilla_type_name=parse_type_name(field_type)),
        proto_file=parse_file_path(field_type),
    )


def parse_type_reference(field: descriptor.FieldDescriptor) -> type_ref.TypeReference:
    """Parses `field` type, not applying any wrappings or replacements yet."""
    type_kind = typing.cast(int, field.type)
    if builtin_type := BUILTIN_TYPES.get(type_kind):
        return builtin_type

    if type_kind == descriptor.FieldDescriptor.TYPE_ENUM:
        enum_type = typing.cast(descriptor.EnumDescriptor, field.enum_type)
        return parse_enum_reference(enum_type)

    if type_kind == descriptor.FieldDescriptor.TYPE_MESSAGE or type_kind == descriptor.FieldDescriptor.TYPE_GROUP:
        # Details on groups:
        # https://protobuf.com/docs/descriptors#groups
        message_type = typing.cast(descriptor.Descriptor, field.message_type)
        return parse_struct_reference(message_type)

    raise RuntimeError(f'Invalid field type kind: {type_kind}')


def handle_type_label(
    field: descriptor.FieldDescriptor,
    parsed_type: type_ref.TypeReference,
) -> type_ref.TypeReference:
    """Applies `optional` and `repeated` labels to the parsed type."""
    label = typing.cast(int, field.label)
    # Message fields are serialized as a sequence of (field number, field value). Some fields may be omitted.
    # * `LABEL_REQUIRED` only exists in proto2. It means that the field is always serialized.
    # * In proto3, all fields except `repeated` are `LABEL_OPTIONAL`, meaning that default value
    #   (not present or default-initialized) is not present in the serialized binary form at all.
    if label == descriptor.FieldDescriptor.LABEL_OPTIONAL:
        if _should_wrap_in_optional(field):
            return type_ref_consts.make_optional(parsed_type)
        else:
            return parsed_type
    if label == descriptor.FieldDescriptor.LABEL_REQUIRED:
        return parsed_type
    if label == descriptor.FieldDescriptor.LABEL_REPEATED:
        return type_ref_consts.make_vector(parsed_type)
    raise RuntimeError(f'Unknown type label {label}')


def _should_wrap_in_optional(field: descriptor.FieldDescriptor) -> bool:
    """
    Policy:
    * Primitive fields are wrapped in `std::optional` iff `has_presence`.
    * Message-typed fields are wrapped in `std::optional` iff defined using an explicit `optional` keyword.
    """
    field_kind: int = field.type
    if field_kind == descriptor.FieldDescriptor.TYPE_MESSAGE or field_kind == descriptor.FieldDescriptor.TYPE_GROUP:
        field_proto = descriptor_proto.to_field_descriptor_proto(field)  # pyright: ignore
        # `proto3_optional` checks whether the field is defined in the `.proto` file using `optional` keyword.
        # Note that message-typed fields are always `has_presence == true`, and `optional` keyword does not affect it.
        # https://github.com/protocolbuffers/protobuf/blob/v22.0/src/google/protobuf/descriptor.proto#L240
        has_optional_keyword = typing.cast(bool, field_proto.proto3_optional)  # pyright: ignore
        return has_optional_keyword
    else:
        # `has_presence` is the proper flag to check `std::optional`-ity in proto3.
        # If `true`, then the field distinguishes unpopulated and default values.
        has_presence = typing.cast(bool, field.has_presence)
        return has_presence


def replace_well_known_types(parsed_type: type_ref.TypeReference) -> type_ref.TypeReference:
    """Replaces `parsed_type` according to options and built-in notions of well-known types."""
    # TODO(TAXICOMMON-10999): support well-known types
    return parsed_type


def parse_type_name(proto_type: TypeDescriptor) -> names.TypeName:
    return names.make_escaped_type_name(
        package=typing.cast(str, proto_type.file.package),
        outer_type_names=_get_outer_structs_names(proto_type),
        short_name=typing.cast(str, proto_type.name),
    )


def _get_outer_structs_names(proto_type: TypeDescriptor) -> Sequence[str]:
    names_list: List[str] = []
    parent_type = proto_type
    while parent_type := typing.cast(Optional[descriptor.Descriptor], parent_type.containing_type):
        names_list.append(typing.cast(str, parent_type.name))
    return list(reversed(names_list))


def parse_file_path(proto_type: TypeDescriptor) -> pathlib.Path:
    file = typing.cast(descriptor.FileDescriptor, proto_type.file)
    file_name = typing.cast(str, file.name)
    return pathlib.Path(file_name)
