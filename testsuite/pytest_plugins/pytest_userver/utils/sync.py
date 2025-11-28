import asyncio
from collections.abc import Callable
import datetime
from typing import Any

MAX_WAIT_TIME = datetime.timedelta(seconds=30)
ITERATION_PERIOD_SECONDS = 0.5


class NotReady(Exception):
    pass


async def wait_until(func: Callable) -> Any:
    """
    Waits for some external event for MAX_WAIT_TIME and
    calls func every ITERATION_PERIOD_SECONDS.
    If func raises NotReady exception, the waiting continues.
    If func succesfully returns, wait_until() returns the same value.
    After MAX_WAIT_TIME of unsuccesfull checks wait_until()
    raises TimeoutError.

    Example:

    .. code-block:: python

        async def try_to_connect_db():
            if not db_conn_is_ok():
                raise NotReady()

        await wait_until(try_to_connect_db)

    @throws TimeoutError after MAX_WAIT_TIME of unsuccesfull checks
    @note If possible, use @ref testpoint instead. @ref testpoint is
    an explicit message from the server "I'm ready", while wait_until()
    uses an implicit idea "A condition is met, so I can continue".

    @ingroup userver_testsuite
    """
    start = datetime.datetime.now()
    while datetime.datetime.now() - start < MAX_WAIT_TIME:
        try:
            return await func()
        except NotReady:
            await asyncio.sleep(ITERATION_PERIOD_SECONDS)

    raise TimeoutError()
