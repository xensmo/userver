from signal import SIGTERM

import pytest

import samples.greeter_pb2 as greeter_protos

MD_ONE_REQ = ' One'
MD_TWO_REQ = ' Two'
MD_ONE_RES = ' EndOne'
MD_TWO_RES = ' EndTwo'


@pytest.mark.uservice_oneshot
async def test_graceful_shutdown_headers(service_daemon_instance, grpc_client, service_client, dynamic_config):
    params = [
        (True, {'x-envoy-immediate-health-check-fail': ['true']}),
        (False, {'x-envoy-immediate-health-check-fail': ['true']}),
        (True, {'x-hdr1': ['true', 'aaa'], 'x-hdr2': ['1', 'bbb', 'yyyy']}),
        (False, {'x-hdr1': ['true', 'aaa'], 'x-hdr2': ['1', 'bbb', 'yyyy']}),
    ]

    for headers_enabled, headers in params:
        dynamic_config.set_values({'GRACEFUL_SHUTDOWN_HEADERS': {'enabled': headers_enabled, 'headers': headers}})
        await service_client.update_server_state()

        request = greeter_protos.GreetingRequest(name='Python')
        call = grpc_client.SayHello(request)
        response = await call
        assert response.greeting == f'Hello, Python{MD_ONE_REQ}{MD_TWO_REQ}!{MD_TWO_RES}{MD_ONE_RES}'
        assert len(await call.initial_metadata()) == 0
        assert len(await call.trailing_metadata()) == 0

    service_daemon_instance.process.send_signal(SIGTERM)

    for headers_enabled, headers in params:
        dynamic_config.set_values({'GRACEFUL_SHUTDOWN_HEADERS': {'enabled': headers_enabled, 'headers': headers}})
        await service_client.update_server_state()

        request = greeter_protos.GreetingRequest(name='Python')
        call = grpc_client.SayHello(request)
        response = await call
        assert response.greeting == f'Hello, Python{MD_ONE_REQ}{MD_TWO_REQ}!{MD_TWO_RES}{MD_ONE_RES}'

        if headers_enabled:
            assert to_dict(await call.initial_metadata()) == headers
        else:
            assert len(await call.initial_metadata()) == 0

        assert len(await call.trailing_metadata()) == 0

    # After a couple more seconds, the service will start shutting down.
    service_daemon_instance.process.wait()


def to_dict(metadata) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for k, v in metadata:
        result.setdefault(k, []).append(v)

    return result
