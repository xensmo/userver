import dataclasses
import pathlib

from sqldto.common import errors
from sqldto.common import models
from sqldto.postgres import fetcher
from sqldto.postgres import translator
from sqldto.utils import cpp_names
from sqldto.utils import logging
from sqldto.utils import rendering

logger = logging.logger

TEMPLATES_DIR = pathlib.Path(__file__).resolve().parent / 'templates'


@dataclasses.dataclass
class EnumEntry:
    name: str
    cpp_name: str


@dataclasses.dataclass
class EnumToGenerate:
    db_name: str
    cpp_name: str
    entries: list[EnumEntry]


@dataclasses.dataclass
class StructField:
    cpp_name: str
    cpp_type: translator.CppType


@dataclasses.dataclass
class StructToGenerate:
    db_name: str
    cpp_name: str
    fields: list[StructField]


@dataclasses.dataclass
class AliasToGenerate:
    cpp_name_from: str
    cpp_name_to: str


@dataclasses.dataclass
class QueryParamToGenerate:
    cpp_type: translator.CppType


@dataclasses.dataclass
class ReturnStructToGenerate:
    cpp_name: str
    fields: list[StructField]


@dataclasses.dataclass
class QueryToGenerate:
    name: str
    cpp_name: str
    sql_name: str
    args: list[QueryParamToGenerate]
    cardinality: models.QueryCardinality
    returns: ReturnStructToGenerate


def _return_field_name(param: models.ReturnParam, index: int, used: set[str]) -> str:
    base = cpp_names.cpp_identifier_lower(param.name) if param.name else f'field{index}'

    candidate = base
    suffix = 1
    while candidate in used:
        candidate = f'{base}_{suffix}'
        suffix += 1

    used.add(candidate)
    return candidate


def _table_to_type(table: models.Table) -> models.StructType:
    return models.StructType(
        namespace=table.namespace,
        name=f'{table.name}_v{table.version}',
        fields=table.columns,
    )


def _table_like_type(table: models.Table, type_: models.StructType) -> bool:
    table_fields = sorted(table.columns, key=lambda v: v.name)
    type_fields = sorted(type_.fields, key=lambda v: v.name)

    if len(table_fields) != len(type_fields):
        return False

    for table_field, type_field in zip(table_fields, type_fields):
        if (table_field.name, table_field.type) != (type_field.name, type_field.type):
            return False

    return True


class PgGenerator(rendering.GeneratorBase):
    def __init__(
        self,
        namespace: str,
        output_dir: pathlib.Path,
        dump_dir: pathlib.Path,
        clang_format: str | None,
        migrations_dir: pathlib.Path,
        queries_dir: pathlib.Path | None,
        query_files: list[str],
        migrations_output_dir: pathlib.Path | None,
        regen_command: str,
    ) -> None:
        super().__init__(TEMPLATES_DIR, clang_format=clang_format)
        self.namespace: str = namespace
        self.output_headers_dir: pathlib.Path = output_dir / 'include' / namespace
        self.output_sources_dir: pathlib.Path = output_dir / 'src' / namespace
        self.migrations_dir: pathlib.Path = migrations_dir
        self.queries_dir: pathlib.Path | None = queries_dir
        self.migrations_output_dir: pathlib.Path | None = migrations_output_dir

        self.fetcher = fetcher.PgSchemaFetcher(
            dump_dir=dump_dir,
            migrations_dir=self.migrations_dir,
            queries_dir=self.queries_dir,
            query_files=query_files,
            regen_command=regen_command,
        )
        self.fetcher.load()

        self.migrations: list[models.Migration] = self.fetcher.migrations
        self.queries: list[models.Query] = self.fetcher.queries
        self.schemas: models.Schema | None = None

    def dump(self) -> None:
        self.schemas = self.fetcher.fetch()

        migration = PgMigrationGenerator(self).generate()
        for to_generate in [migration]:
            if to_generate:
                super().render(to_generate)

        self.fetcher.load()
        self.fetcher.dump()

    def generate(self) -> None:
        self.schemas = self.fetcher.load_or_error()

        migration = PgMigrationGenerator(self).generate()
        pg_models = PgModelsGenerator(self).generate()
        pg_queries = PgQueriesGenerator(self).generate()

        for to_generate in [migration, pg_models, *pg_queries]:
            if to_generate:
                super().render(to_generate)


class PgMigrationGenerator:
    def __init__(self, parent: PgGenerator):
        self.parent: PgGenerator = parent

    def generate(self) -> rendering.ToGenerate | None:
        if not self.parent.migrations:
            return None

        if not self.parent.migrations_output_dir:
            return None

        types_to_generate: list[models.StructType] = []

        for table in self.parent.schemas.catalog.tables.values():
            if table.type_name not in self.parent.schemas.catalog.types:
                types_to_generate.append(_table_to_type(table))
                continue

            if _table_like_type(table, self.parent.schemas.catalog.types[table.type_name]):
                continue

            logger.warning(
                'Conflict type %s for table %s, skip it',
                table.type_name,
                table.db_name,
            )

        if not types_to_generate:
            return None

        version = self.parent.migrations[-1].next_version()

        return rendering.ToGenerate(
            template_name='VX__codegen_migration.sql.jinja',
            output_file=self.parent.migrations_output_dir / f'V{version}__codegen_migration.sql',
            context={
                'types': types_to_generate,
            },
        )


class PgModelsGenerator:
    def __init__(self, base: PgGenerator):
        self.base = base
        self.catalog = base.schemas.catalog

    def pg_to_cpp_type(
        self,
        pg_object: models.Column | models.StructType | models.DbEnum,
        includes_accumulator: set[str] = set(),
    ) -> translator.CppType:
        pg_typename: str = None
        nullable: bool = None

        if isinstance(pg_object, models.Column):
            pg_typename = pg_object.type
            nullable = pg_object.nullable

        elif isinstance(pg_object, models.StructType):
            pg_typename = pg_object.db_name
            nullable = False

        elif isinstance(pg_object, models.DbEnum):
            pg_typename = pg_object.db_name
            nullable = False

        cpp_type = translator.pg_to_cpp_type(
            pg_typename,
            nullable,
            self.catalog,
        )

        if not cpp_type:
            raise errors.UnexpectedError(f'Unknown type: {pg_typename}')

        if not all(template.value for template in cpp_type.templates):
            missing_templates = [template.name for template in cpp_type.templates if not template.value]
            raise errors.UnsupportedError(
                f"""
                Unsupported postgres type: {pg_typename} that requires templates.
                For {cpp_type.typename} need specify {','.join(missing_templates)}.
                """
            )

        includes_accumulator.update(cpp_type.includes)
        return cpp_type

    def trust_typename(self, pg_typename: str) -> bool:
        if pg_typename not in self.catalog.types:
            return True

        pg_type = self.catalog.types[pg_typename]

        for table in self.catalog.tables.values():
            if table.type_name == pg_typename and _table_like_type(table, pg_type):
                return True

        return False

    def generate(self) -> rendering.ToGenerate | None:
        includes_to_generate: set[str] = set()
        user_types = [pg_type for pg_type in self.catalog.sorted_types() if not self.trust_typename(pg_type.db_name)]
        table_types = [
            _table_to_type(table) for table in self.catalog.sorted_tables() if self.trust_typename(table.type_name)
        ]

        enums_to_generate = [
            EnumToGenerate(
                db_name=enum.db_name,
                cpp_name=translator.pg_name_to_cpp_name(enum.db_name),
                entries=[
                    EnumEntry(
                        name=value,
                        cpp_name=cpp_names.cpp_enum_entry(value),
                    )
                    for value in enum.values
                ],
            )
            for enum in self.catalog.sorted_enums()
        ]

        structs_to_generate = [
            StructToGenerate(
                db_name=type_.db_name,
                cpp_name=translator.pg_name_to_cpp_name(type_.db_name),
                fields=[
                    StructField(
                        cpp_name=cpp_names.cpp_identifier_lower(field.name),
                        cpp_type=self.pg_to_cpp_type(field, includes_to_generate),
                    )
                    for field in type_.fields
                ],
            )
            for type_ in user_types + table_types
        ]

        aliases_to_generate = [
            AliasToGenerate(
                cpp_name_from=translator.pg_name_to_cpp_name(table.db_name),
                cpp_name_to=translator.pg_name_to_cpp_name(_table_to_type(table).db_name),
            )
            for table in self.catalog.sorted_tables()
            if self.trust_typename(table.type_name)
        ]

        return rendering.ToGenerate(
            template_name='pg_models.hpp.jinja',
            output_file=self.base.output_headers_dir / 'pg_models.hpp',
            context={
                'namespace': self.base.namespace,
                'includes': list(sorted(set(includes_to_generate))),
                'structs': structs_to_generate,
                'enums': enums_to_generate,
                'aliases': aliases_to_generate,
            },
            clang_format=True,
        )


class PgQueriesGenerator:
    def __init__(self, base: PgGenerator):
        self.base = base
        self.catalog = base.schemas.catalog

    def pg_to_cpp_type(
        self,
        param: models.QueryParam,
        includes_accumulator: set[str] = set(),
    ) -> list[translator.CppType]:
        cpp_type = translator.pg_to_cpp_type(param.type, param.nullable, self.catalog)

        if not cpp_type:
            raise errors.UnexpectedError(f'Unknown type: {param.type}')

        if not all(template.value for template in cpp_type.templates):
            missing_templates = [template.name for template in cpp_type.templates if not template.value]
            raise errors.UnsupportedError(
                f"""
                Unsupported postgres query parameter type: {param.type} that requires templates.
                For {cpp_type.typename} need specify {','.join(missing_templates)}.
                Or you can use @arg<N>: <type> to override the type.
                """
            )

        includes_accumulator.update(cpp_type.includes)
        return cpp_type

    def generate(self) -> list[rendering.ToGenerate]:
        if not self.base.schemas.queries:
            return []

        includes_to_generate: set[str] = {
            '<userver/storages/postgres/cluster.hpp>',
            '<userver/storages/postgres/postgres_fwd.hpp>',
        }

        queries_to_generate: list[QueryToGenerate] = []
        for query in self.base.schemas.queries:
            args = [
                QueryParamToGenerate(
                    cpp_type=self.pg_to_cpp_type(param, includes_to_generate),
                )
                for param in query.args
            ]

            used_field_names: set[str] = set()
            returns = ReturnStructToGenerate(
                cpp_name=cpp_names.cpp_identifier_camel_case(query.name) + 'Row',
                fields=[
                    StructField(
                        cpp_name=_return_field_name(param, index, used_field_names),
                        cpp_type=self.pg_to_cpp_type(param, includes_to_generate),
                    )
                    for index, param in enumerate(query.returns)
                ],
            )

            if len(returns.fields) > 1:
                includes_to_generate.add(
                    '<userver/storages/postgres/io/row_types.hpp>',
                )

            queries_to_generate.append(
                QueryToGenerate(
                    name=query.name,
                    cpp_name=cpp_names.cpp_identifier_camel_case(query.name),
                    sql_name=cpp_names.cpp_enum_entry(query.name),
                    args=args,
                    cardinality=query.cardinality,
                    returns=returns,
                )
            )

        context = {
            'namespace': self.base.namespace,
            'includes': list(sorted(set(includes_to_generate))),
            'queries': queries_to_generate,
        }

        return [
            rendering.ToGenerate(
                template_name='pg_client.hpp.jinja',
                output_file=self.base.output_headers_dir / 'pg_client.hpp',
                context=context,
                clang_format=True,
            ),
            rendering.ToGenerate(
                template_name='pg_cluster.hpp.jinja',
                output_file=self.base.output_headers_dir / 'pg_cluster.hpp',
                context=context,
                clang_format=True,
            ),
            rendering.ToGenerate(
                template_name='pg_mock.hpp.jinja',
                output_file=self.base.output_headers_dir / 'pg_mock.hpp',
                context=context,
                clang_format=True,
            ),
            rendering.ToGenerate(
                template_name='pg_cluster.cpp.jinja',
                output_file=self.base.output_sources_dir / 'pg_cluster.cpp',
                context=context,
                clang_format=True,
            ),
        ]
