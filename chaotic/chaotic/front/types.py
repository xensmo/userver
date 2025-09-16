import collections
import dataclasses
import enum
import typing
from typing import Any
from typing import Callable
from typing import Optional
from typing import TypeVar
from typing import Union

import pydantic

from chaotic import error
from . import base_model


class FieldError(Exception):
    def __init__(self, field: str, msg: str) -> None:
        self.field = field
        self.msg = msg


@dataclasses.dataclass(frozen=True)
class SourceLocation:
    # e.g. some/file/name.yaml
    filepath: str
    # e.g. /definitions/Type
    location: str

    def __str__(self) -> str:
        return f'{self.filepath}#{self.location}'


_OptionalBool = TypeVar('_OptionalBool', bool, Optional[bool])
_OptionalStr = TypeVar('_OptionalStr', str, Optional[str])


class Schema(base_model.BaseModel):
    title: str = ''
    description: Optional[str] = ''
    example: Any = ''

    source_location_: Optional[SourceLocation] = pydantic.Field(
        exclude=True,
        default=None,
    )

    @classmethod
    def model_userver_tags(cls) -> list[str]:
        return [
            'x-usrv-cpp-typedef-tag',
            'x-taxi-cpp-typedef-tag',
            'x-usrv-cpp-type',
            'x-taxi-cpp-type',
            'x-usrv-cpp-name',
            'x-taxi-cpp-name',
        ]

    def visit_children(self, cb: Callable[['Schema', 'Schema'], None]) -> None:
        pass

    def __hash__(self) -> int:
        return id(self)

    def source_location(self) -> SourceLocation:
        assert self.source_location_
        return self.source_location_

    def delete_x_property(self, name: str) -> None:
        if name in self.x_properties:
            del self.x_properties[name]

    def get_x_property_str(
        self,
        name: str,
        default: _OptionalStr = None,
    ) -> _OptionalStr:
        return self._get_x_property_typed(name, default, str, 'string')

    def get_x_property_bool(
        self,
        name: str,
        default: _OptionalBool = None,
    ) -> _OptionalBool:
        return self._get_x_property_typed(name, default, bool, 'boolean')

    def _get_x_property_typed(
        self,
        name: str,
        default: Any,
        type_: type,
        type_str: str,
    ) -> Any:
        value = self.x_properties.get(name, default)
        if value is None:
            return None

        if isinstance(value, type_):
            return value

        raise error.BaseError(
            full_filepath=self.source_location().filepath,
            infile_path=self.source_location().location,
            schema_type='jsonschema',
            msg=f'Non-{type_str} x- property "{name}"',
        )


_NOT_IMPL = Schema()


class Ref(Schema):
    ref: str  # type: ignore
    indirect: bool
    self_ref: bool
    schema_: Schema = _NOT_IMPL

    def model_post_init(self, context: Any):
        super().model_post_init(context)
        assert self.ref.find('/../') == -1, self.ref

    __hash__ = Schema.__hash__


class Boolean(Schema):
    type: str = 'boolean'
    default: Optional[bool] = None
    nullable: bool = False
    deprecated: bool = False

    __hash__ = Schema.__hash__


# TODO: default & nullable => raise


class IntegerFormat(enum.Enum):
    INT32 = 32
    INT64 = 64

    @classmethod
    def from_string(cls, data: str) -> 'IntegerFormat':
        fmt = INTEGER_FORMAT_TO_FORMAT.get(data)
        if not fmt:
            raise Exception(f'Unknown format: {data}')
        return fmt


INTEGER_FORMAT_TO_FORMAT = {
    'int32': IntegerFormat.INT32,
    'int64': IntegerFormat.INT64,
}


class Integer(Schema):
    type: str = 'integer'
    default: Optional[int] = None
    nullable: bool = False
    minimum: Optional[int] = None
    maximum: Optional[int] = None
    exclusiveMinimum: Optional[Union[int, bool]] = None
    exclusiveMaximum: Optional[Union[int, bool]] = None
    # TODO: multipleOf
    enum: Optional[list[int]] = None
    format: Optional[IntegerFormat] = None
    deprecated: bool = False

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if self.enum:
            for item in self.enum:
                if not isinstance(item, int):
                    raise FieldError(
                        'enum',
                        f'field "enum" contains non-integers ({item})',
                    )

    __hash__ = Schema.__hash__


class Number(Schema):
    type: str = 'number'
    default: Optional[Union[float, int]] = None
    nullable: bool = False
    minimum: Optional[Union[float, int]] = None
    maximum: Optional[Union[float, int]] = None
    exclusiveMinimum: Optional[Union[float, int, bool]] = None
    exclusiveMaximum: Optional[Union[float, int, bool]] = None
    format: Optional[str] = None
    deprecated: bool = False
    # TODO: multipleOf

    __hash__ = Schema.__hash__


class StringFormat(enum.Enum):
    BINARY = enum.auto()
    BYTE = enum.auto()
    DATE = enum.auto()
    DATE_TIME = enum.auto()
    DATE_TIME_ISO_BASIC = enum.auto()
    DATE_TIME_FRACTION = enum.auto()
    UUID = enum.auto()
    URI = enum.auto()

    @classmethod
    def from_string(cls, data: str) -> 'StringFormat':
        fmt = STRING_FORMAT_TO_FORMAT.get(data)
        if not fmt:
            raise Exception(f'Unknown format: {data}')
        return fmt


STRING_FORMAT_TO_FORMAT = {
    'binary': StringFormat.BINARY,
    'byte': StringFormat.BYTE,
    'date': StringFormat.DATE,
    'date-time': StringFormat.DATE_TIME,
    'date-time-iso-basic': StringFormat.DATE_TIME_ISO_BASIC,
    'date-time-fraction': StringFormat.DATE_TIME_FRACTION,
    'uuid': StringFormat.UUID,
    'uri': StringFormat.URI,
}


class String(Schema):
    type: str = 'string'
    default: Optional[str] = None
    nullable: bool = False
    enum: Optional[list[str]] = None
    pattern: Optional[str] = None
    format: Optional[StringFormat] = None
    minLength: Optional[int] = None
    maxLength: Optional[int] = None
    deprecated: bool = False

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if self.enum:
            for item in self.enum:
                if not isinstance(item, str):
                    raise FieldError(
                        'enum',
                        f'field "enum" contains non-strings ({item})',
                    )

    __hash__ = Schema.__hash__


class Array(Schema):
    # validated in SchemaParser._parse_array()
    items: Schema
    type: str = 'array'
    nullable: bool = False
    minItems: Optional[int] = None
    maxItems: Optional[int] = None
    deprecated: bool = False

    @classmethod
    def model_userver_tags(cls) -> list[str]:
        return Schema.model_userver_tags() + [
            'x-taxi-cpp-container',
            'x-usrv-cpp-container',
        ]

    def visit_children(self, cb: Callable[[Schema, Schema], None]) -> None:
        cb(self.items, self)
        self.items.visit_children(cb)

    __hash__ = Schema.__hash__


class SchemaObject(Schema):
    type_: str = pydantic.Field(alias='type', default='object')
    additionalProperties: Union[Schema, bool]
    properties: dict[str, Schema]
    required: Optional[list[str]] = None
    nullable: bool = False
    deprecated: bool = False

    @classmethod
    def model_userver_tags(cls) -> list[str]:
        return Schema.model_userver_tags() + [
            'x-taxi-cpp-extra-member',
            'x-usrv-cpp-extra-member',
            'x-taxi-strict-parsing',
            'x-usrv-strict-parsing',
            'x-taxi-cpp-extra-type',
            'x-usrv-cpp-extra-type',
            'x-taxi-additional-properties-true-reason',
        ]

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if self.required:
            for name in self.required:
                if name not in self.properties:
                    raise FieldError(
                        'required',
                        f'Field "{name}" is set in "required", but missing in "properties"',
                    )

    def visit_children(self, cb: Callable[[Schema, Schema], None]) -> None:
        if isinstance(self.additionalProperties, Schema):
            cb(self.additionalProperties, self)
            self.additionalProperties.visit_children(cb)

        for prop in self.properties.values():
            cb(prop, self)
            prop.visit_children(cb)

    __hash__ = Schema.__hash__


class AllOf(Schema):
    allOf: list[Schema]  # type: ignore
    nullable: bool = False

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if not self.allOf:
            raise FieldError('allOf', 'Empty allOf')

    def visit_children(self, cb: Callable[[Schema, Schema], None]) -> None:
        for parent in self.allOf:
            cb(parent, self)
            parent.visit_children(cb)

    __hash__ = Schema.__hash__


class OneOfWithoutDiscriminator(Schema):
    oneOf: list[Schema]  # type:ignore
    nullable: bool = False

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if not self.oneOf:
            raise FieldError('oneOf', 'Empty oneOf')

    def visit_children(self, cb: Callable[[Schema, Schema], None]) -> None:
        for variant in self.oneOf:
            cb(variant, self)
            variant.visit_children(cb)

    __hash__ = Schema.__hash__


class MappingType(enum.Enum):
    STR = 0
    INT = 1


class DiscMapping(base_model.BaseModel):
    # only one list must be not none
    str_values: Optional[list[list[str]]] = None
    int_values: Optional[list[list[int]]] = None

    def append(self, value: list):
        if self.str_values is not None:
            self.str_values.append(typing.cast(list[str], value))
        elif self.int_values is not None:
            self.int_values.append(typing.cast(list[int], value))

    def enable_str(self):
        self.str_values = []
        self.int_values = None

    def enable_int(self):
        self.int_values = []
        self.str_values = None

    def get_type(self) -> MappingType:
        if self.is_int():
            return MappingType.INT

        return MappingType.STR

    def as_strs(self) -> list[list[str]]:
        return typing.cast(list[list[str]], self.str_values)

    def as_ints(self) -> list[list[int]]:
        return typing.cast(list[list[int]], self.int_values)

    def is_int(self):
        return self.int_values is not None

    def is_str(self):
        return self.str_values is not None


class OneOfWithDiscriminator(Schema):
    oneOf: list[Ref]  # type:ignore
    discriminator_property: Optional[str] = None
    mapping: DiscMapping = pydantic.Field(default_factory=DiscMapping)
    nullable: bool = False

    def model_post_init(self, context: Any) -> None:
        super().model_post_init(context)
        if not self.oneOf:
            raise FieldError('oneOf', 'Empty oneOf')

    def visit_children(self, cb: Callable[[Schema, Schema], None]) -> None:
        for variant in self.oneOf:
            cb(variant, self)
            variant.visit_children(cb)

    __hash__ = Schema.__hash__


class OneOfDiscriminatorRaw(base_model.BaseModel):
    propertyName: str  # type:ignore
    mapping: Optional[dict[Union[str, int], str]] = None


@dataclasses.dataclass
class ParsedSchemas:
    schemas: dict[str, Schema] = dataclasses.field(
        default_factory=collections.OrderedDict,
    )

    @staticmethod
    def merge(schemas: list['ParsedSchemas']) -> 'ParsedSchemas':
        result = ParsedSchemas()
        for schema in schemas:
            result.schemas.update(schema.schemas)
        return result


@dataclasses.dataclass
class ResolvedSchemas:
    schemas: dict[str, Schema]
