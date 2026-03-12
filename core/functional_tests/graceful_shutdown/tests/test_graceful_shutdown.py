import asyncio

import pytest_userver.utils.sync as sync


async def wait_for_daemon_stop(_global_daemon_store):
    def is_ready():
        return not _global_daemon_store.has_running_daemons()

    await sync.wait(is_ready)

    assert not _global_daemon_store.has_running_daemons(), 'Daemon has not stopped'
    await _global_daemon_store.aclose()


async def test_graceful_shutdown_timer_and_header(
    service_client,
    monitor_client,
    dynamic_config,
    _global_daemon_store,
):
    params = [
        (True, {'x-envoy-immediate-health-check-fail': ['true']}),
        (False, {'x-envoy-immediate-health-check-fail': ['true']}),
        (True, {'x-hdr1': ['true', 'aaa'], 'x-hdr2': ['1', 'bbb', 'yyyy']}),
        (False, {'x-hdr1': ['true', 'aaa'], 'x-hdr2': ['1', 'bbb', 'yyyy']}),
    ]

    response = await service_client.get('/ping')
    assert response.status == 200

    for headers_enabled, headers in params:
        dynamic_config.set_values({'GRACEFUL_SHUTDOWN_HEADERS': {'enabled': headers_enabled, 'headers': headers}})
        await service_client.update_server_state()

        response = await monitor_client.get('/log-level/')
        assert response.status == 200
        for header, _ in headers.items():
            assert response.headers.get(header) is None

    response = await monitor_client.post('/sigterm')
    assert response.status == 200

    response = await service_client.get('/ping')
    assert response.status == 500

    for headers_enabled, headers in params:
        dynamic_config.set_values({'GRACEFUL_SHUTDOWN_HEADERS': {'enabled': headers_enabled, 'headers': headers}})
        await service_client.update_server_state()

        response = await monitor_client.get('/log-level/')
        assert response.status == 200
        if headers_enabled:
            for header, values in headers.items():
                assert response.headers.get(header) == ', '.join(values)
        else:
            for header, _ in headers.items():
                assert response.headers.get(header) is None

    await asyncio.sleep(2)
    # Check that the service is still alive.
    response = await service_client.get('/ping')
    assert response.status == 500

    # After a couple more seconds, the service will start shutting down.
    await wait_for_daemon_stop(_global_daemon_store)
