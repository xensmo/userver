import tempfile

import psycopg2

from testsuite.databases.pgsql import service
from testsuite.environment.service import ScriptService

from sqldto.utils import logging

logger = logging.logger


class PgRunner:
    def __init__(
        self,
        service_name='',
    ) -> None:
        self._service_name = service_name
        self._pg_settings = service.get_service_settings()
        self._working_dir: tempfile.TemporaryDirectory | None = None
        self._pg_service: ScriptService | None = None

    def connect(self):
        return psycopg2.connect(
            host=self._pg_settings.get_conninfo().host,
            port=self._pg_settings.get_conninfo().port,
            dbname='postgres',
            user=self._pg_settings.get_conninfo().user,
        )

    def start(self, verbose=0) -> None:
        self._working_dir = tempfile.TemporaryDirectory()
        self._pg_service = service.create_pgsql_service(
            service_name=self._service_name,
            working_dir=self._working_dir.name,
            settings=self._pg_settings,
        )
        self._pg_service.ensure_started(verbose=verbose)
        logger.debug('PostgreSQL URI: %s', self._pg_settings.get_conninfo().get_uri())

    def stop(self, verbose=0) -> None:
        if self._pg_service:
            self._pg_service.stop(verbose=verbose)

        if self._working_dir:
            self._working_dir.cleanup()

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        self.stop()
