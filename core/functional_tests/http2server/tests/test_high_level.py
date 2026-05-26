import asyncio
import random
import string

import pytest_userver.utils.sync as sync

DEFAULT_PATH = '/http2server'
DEFAULT_DATA = {'hello': 'world'}


async def test_http1_ping(service_client):
    r = await service_client.get('/ping')
    assert r.status == 200


async def test_http2_ping(http2_client):
    r = await http2_client.get('/ping', timeout=1)
    assert 200 == r.status_code
    assert '' == r.text


async def test_big_body(http2_client):
    big_body = 'x' * 2**22  # request - 4Mib. limit - 2Mib
    r = await http2_client.get(
        DEFAULT_PATH,
        params={'type': 'echo-body'},
        data=big_body,
    )
    assert 413 == r.status_code
    assert 'too large request' == r.text

    big_body = 'x' * 2**20  # request - 1Mib. limit - 2Mib
    r = await http2_client.get(
        DEFAULT_PATH,
        params={'type': 'echo-body'},
        data=big_body,
    )
    assert 200 == r.status_code
    assert big_body == r.text


async def test_body_different_size(http2_client):
    data = ''
    for _ in range(1026):
        r = await http2_client.get(
            DEFAULT_PATH,
            params={'type': 'echo-body'},
            data=data,
        )
        assert 200 == r.status_code
        assert data == r.text
        data += 'x'


async def test_json_body(http2_client):
    data = {'x': 'X', 'y': 'Y', 'd': 0.123, 'b': True, 'arr': [1, 2, 3, 4]}
    r = await http2_client.get(
        DEFAULT_PATH,
        params={'type': 'json'},
        json=data,
    )
    assert 200 == r.status_code
    assert data == r.json()


async def test_headers(http2_client):
    hval = 'val'
    r = await http2_client.post(
        DEFAULT_PATH,
        params={'type': 'echo-header'},
        headers={'echo-header': hval, 'test': 'test'},
        json=DEFAULT_DATA,
    )
    assert 200 == r.status_code
    assert hval == r.text


async def test_head_response_has_no_body(http2_client):
    r = await http2_client.head(
        DEFAULT_PATH,
        params={'type': 'echo-header'},
        headers={'echo-header': 'body-that-must-not-be-sent'},
    )
    assert 200 == r.status_code
    assert '' == r.text


async def _get_metric(monitor_client, metric_name):
    metric = await monitor_client.single_metric(
        f'server.requests.http2.{metric_name}',
    )
    return metric.value


async def _get_http2_metrics(monitor_client):
    metric_names = (
        'streams-count',
        'streams-close',
        'reset-streams',
        'goaway',
        'streams-parse-error',
    )
    return {metric_name: await _get_metric(monitor_client, metric_name) for metric_name in metric_names}


def _random_string(bytes_count=128):
    ascii_chars = string.printable.strip()
    return ''.join(random.choices(ascii_chars, k=bytes_count))


async def _do_echo_request(client, bytes_count=128):
    data = _random_string(bytes_count)
    r = await client.put(
        DEFAULT_PATH,
        params={'type': 'echo-body'},
        data=data,
    )
    assert 200 == r.status_code
    assert data == r.text


async def test_concurrent_requests(
    http2_client,
    service_client,
    monitor_client,
):
    await service_client.update_server_state()

    initial_http2_metrics = await _get_http2_metrics(monitor_client)

    tcp_connections_start = await monitor_client.single_metric('server.connections.opened.v2')
    tcp_connections_start = tcp_connections_start.value

    requests_count = 200
    tasks = [_do_echo_request(http2_client) for _ in range(requests_count)]
    await asyncio.gather(*tasks)

    await service_client.update_server_state()

    tcp_connections_end = await monitor_client.single_metric('server.connections.opened.v2')
    tcp_connections_end = tcp_connections_end.value

    assert tcp_connections_end == tcp_connections_start + 1

    async def is_ready():
        def expect_eq(a, b):
            if a == b:
                return
            raise sync.NotReady

        total_requests = requests_count + initial_http2_metrics['streams-count']
        expect_eq(total_requests, await _get_metric(monitor_client, 'streams-count'))
        expect_eq(
            requests_count + initial_http2_metrics['streams-close'],
            await _get_metric(monitor_client, 'streams-close'),
        )
        expect_eq(
            initial_http2_metrics['reset-streams'],
            await _get_metric(monitor_client, 'reset-streams'),
        )
        expect_eq(
            initial_http2_metrics['goaway'],
            await _get_metric(monitor_client, 'goaway'),
        )
        expect_eq(
            initial_http2_metrics['streams-parse-error'],
            await _get_metric(monitor_client, 'streams-parse-error'),
        )

        return True

    await sync.wait(is_ready)


async def test_concurrent_requests_with_big_body(
    http2_client,
    service_client,
    monitor_client,
):
    await service_client.update_server_state()

    initial_http2_metrics = await _get_http2_metrics(monitor_client)

    requests_count = 50
    tasks = [_do_echo_request(http2_client, 2**20) for _ in range(requests_count)]
    await asyncio.gather(*tasks)

    await service_client.update_server_state()

    metrics = await monitor_client.metrics(prefix='server.requests.http2')
    assert len(metrics) == 5
    total_requests = requests_count + initial_http2_metrics['streams-count']
    assert total_requests == await _get_metric(monitor_client, 'streams-count')
    assert requests_count + initial_http2_metrics['streams-close'] == await _get_metric(
        monitor_client,
        'streams-close',
    )
    assert initial_http2_metrics['reset-streams'] == await _get_metric(
        monitor_client,
        'reset-streams',
    )
    assert initial_http2_metrics['goaway'] == await _get_metric(
        monitor_client,
        'goaway',
    )
    assert initial_http2_metrics['streams-parse-error'] == await _get_metric(
        monitor_client,
        'streams-parse-error',
    )
