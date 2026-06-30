from hashlib import sha256
import json
import pathlib

from sqldto.common import errors
from sqldto.common import migrations
from sqldto.common import models
from sqldto.common import queries
from sqldto.postgres import runner
from sqldto.postgres.runtime import catalog_analyzer
from sqldto.postgres.runtime import queries_analyzer


class PgSchemaFetcher:
    def __init__(
        self,
        dump_dir: pathlib.Path,
        migrations_dir: pathlib.Path,
        queries_dir: pathlib.Path | None,
        query_files: list[str] | None,
        regen_command: str,
    ) -> None:
        self.dump_file: pathlib.Path = dump_dir / 'schema.dto.json'
        self.migrations_dir: pathlib.Path = migrations_dir
        self.queries_dir: pathlib.Path | None = queries_dir
        self.query_files: list[str] | None = query_files
        self.regen_command: str = regen_command

        self.migrations: list[models.Migration] = []
        self.queries: list[models.Query] = []

    def load(self) -> None:
        self.migrations = migrations.load(self.migrations_dir)

        if self.queries_dir is not None and self.query_files is not None:
            self.queries = queries.load(self.queries_dir, self.query_files)

    def hashsum(self) -> str:
        source = {
            'migrations': [
                {
                    'name': migration.path.name,
                    'sql': migration.sql,
                }
                for migration in self.migrations
            ],
            'queries': [
                {
                    'name': query.path.name,
                    'sql': query.sql,
                }
                for query in self.queries
            ],
        }
        return sha256(json.dumps(source, sort_keys=True).encode()).hexdigest()

    def load_cached(self) -> models.Schema | None:
        hashsum = self.hashsum()

        if self.dump_file.is_file():
            dump = json.loads(self.dump_file.read_text())
            schema = models.Schema.from_dict(dump)
            if schema.hashsum == hashsum:
                return schema

        return None

    def fetch(self) -> models.Schema:
        with runner.PgRunner() as pg, pg.connect() as conn, conn.cursor() as cursor:
            pg_catalog_analyzer = catalog_analyzer.PgCatalogAnalyzer()
            pg_catalog_analyzer.apply_migrations(cursor, self.migrations)
            catalog = pg_catalog_analyzer.catalog

            conn.commit()

            pg_query_analyzer = queries_analyzer.PgQueryAnalyzer(catalog)
            queries = pg_query_analyzer.read_schemas(cursor, self.queries)
            return models.Schema(
                hashsum=self.hashsum(),
                catalog=catalog,
                queries=queries,
            )

    def dump(self) -> models.Schema:
        schema = self.fetch()
        self.dump_file.write_text(
            json.dumps(
                schema.to_dict(),
                sort_keys=True,
                indent=2,
                separators=(',', ': '),
                ensure_ascii=False,
            )
            + '\n'
        )
        return schema

    def load_or_error(self) -> models.Schema:
        if not self.dump_file.is_file():
            raise errors.ClientError(f"""
            DTO schema dump not found: {self.dump_file}
            Generate it by running:
            {self.regen_command}
            """)

        if schema := self.load_cached():
            return schema

        raise errors.ClientError(f"""
        DTO schema dump is outdated: {self.dump_file}
        Regenerate it by running:
        {self.regen_command}
        """)
