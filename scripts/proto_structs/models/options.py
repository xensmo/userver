"""
Options for proto-structs to generate.
These can come from the proto file, as well as passed to the protoc plugin by the build system.
"""

import dataclasses
import json
import pathlib
from typing import Annotated
from typing import Any
from typing import Dict
from typing import Optional
from typing import TypeVar

import pydantic


class FileOptions(pydantic.BaseModel, extra='forbid'):
    """Options that apply to all definitions or types in a file."""


class MessageOptions(pydantic.BaseModel, extra='forbid'):
    """Options that apply to struct definition or to all fields typed with this struct."""

    #: True, if the struct's type should always be wrapped in `utils::Box`.
    indirect: bool = False


class OneofOptions(pydantic.BaseModel, extra='forbid'):
    """Options that apply to oneof definition."""


class FieldOptions(pydantic.BaseModel, extra='forbid'):
    """Options that apply to message fields."""

    #: True, if the field's type should be wrapped in `utils::Box`.
    indirect: bool = False


FullName = Annotated[str, pydantic.StringConstraints(pattern=r'^\w+(\.\w+)*$')]


class PluginOptions(pydantic.BaseModel, extra='forbid'):
    """Options root."""

    #: Proto file path (as in `import`) `full/file/name.proto` -> options.
    file_options: Dict[FullName, FileOptions] = dataclasses.field(default_factory=dict)

    #: Full message name in format `full.package.Message.SubMessage` -> options.
    message_options: Dict[FullName, MessageOptions] = dataclasses.field(default_factory=dict)

    #: Full oneof name in format `full.package.Message.Submessage.oneof_name` -> options.
    oneof_options: Dict[FullName, OneofOptions] = dataclasses.field(default_factory=dict)

    #: Full field name in format `full.package.Message.SubMessage.field` -> options.
    field_options: Dict[FullName, FieldOptions] = dataclasses.field(default_factory=dict)


# Some options for common plugins are there to avoid requiring passing the options in cmake for common library schemas.
_DEFAULT_PLUGIN_OPTIONS = PluginOptions(
    field_options={
        # Built-in types.
        'google.protobuf.Struct.fields': FieldOptions(indirect=True),
        # Well-known types.
        'google.api.BackendRule.overrides_by_request_protocol': FieldOptions(indirect=True),
        # Protovalidate types.
        'buf.validate.RepeatedRules.items': FieldOptions(indirect=True),
        'buf.validate.MapRules.keys': FieldOptions(indirect=True),
        'buf.validate.MapRules.values': FieldOptions(indirect=True),
    },
)


def _merge_dicts_recursively(*, into: Dict[str, Any], overrides: Dict[str, Any]) -> None:
    for key, value in overrides.items():
        if key in into and isinstance(into[key], dict) and isinstance(value, dict):
            sub_into: Dict[str, Any] = into[key]
            sub_overrides: Dict[str, Any] = value
            _merge_dicts_recursively(into=sub_into, overrides=sub_overrides)
        else:
            into[key] = value


TBaseModelSubtype = TypeVar('TBaseModelSubtype', bound=pydantic.BaseModel)


def merge_models(base: TBaseModelSubtype, *, overrides: TBaseModelSubtype) -> TBaseModelSubtype:
    """Merge two pydantic models recursively."""
    assert base.__class__ == overrides.__class__
    merged = base.model_dump()
    _merge_dicts_recursively(into=merged, overrides=overrides.model_dump())
    return base.__class__(**merged)


def load_plugin_options(file_path: Optional[pathlib.Path]) -> PluginOptions:
    """Load `PluginOptions` from file."""
    if not file_path:
        return _DEFAULT_PLUGIN_OPTIONS

    with open(file_path, 'r', encoding='utf-8') as file:
        options = PluginOptions(**json.load(file))
    return merge_models(_DEFAULT_PLUGIN_OPTIONS, overrides=options)
