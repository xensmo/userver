import asyncio

import aiohttp
import pytest


@pytest.mark.skip(reason='the feature is not yet enabled')
async def test_monitor_port_is_open_before_all_components_are_ready(
    ensure_daemon_started,
    service_daemon_scope,
    service_baseurl,
    monitor_baseurl,
):
    daemon_task = asyncio.create_task(ensure_daemon_started(service_daemon_scope))

    # call /monitor
    async with aiohttp.ClientSession() as session:
        for i in range(120):
            try:
                response = await session.get(monitor_baseurl + 'monitor')
                if response.status == 200:
                    break
                await asyncio.sleep(0.5)
            except aiohttp.ClientConnectorError:
                await asyncio.sleep(0.5)
        else:
            assert False, 'Service has not started'

    # make sure /ping is now ready
    await daemon_task
