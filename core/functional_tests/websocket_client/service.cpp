#include <fmt/format.h>
#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/clients/http/websocket_response.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/engine/wait_any.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/server/handlers/websocket_handler.hpp>
#include <userver/testsuite/testsuite_support.hpp>
#include <userver/utils/async.hpp>
#include <userver/utils/daemon_run.hpp>

#include <userver/utest/using_namespace_userver.hpp>

// Echo handler for testing client
class WebSocketEcho final : public server::handlers::WebsocketHandlerBase {
public:
    static constexpr std::string_view kName = "websocket-echo-handler";

    using WebsocketHandlerBase::WebsocketHandlerBase;

    void Handle(websocket::WebSocketConnection& ws, server::request::RequestContext&) const override {
        websocket::Message message;
        while (!engine::current_task::ShouldCancel()) {
            ws.Recv(message);

            if (message.close_status) {
                ws.Close(*message.close_status);
                break;
            }

            // Echo back
            ws.Send(message);
        }
    }
};

// HTTP handler for testing C++ WebSocket client
class TestClientHandler final : public server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "test-client";

    TestClientHandler(const components::ComponentConfig& config, const components::ComponentContext& context)
        : HttpHandlerBase(config, context),
          client_(context.FindComponent<components::HttpClient>().GetHttpClient())
    {}

    std::string HandleRequestThrow(const server::http::HttpRequest& request, server::request::RequestContext&)
        const override {
        const auto& test_name = request.GetArg("test");

        try {
            if (test_name == "echo") {
                return TestEcho(request);
            } else if (test_name == "large") {
                return TestLarge(request);
            } else if (test_name == "multiple") {
                return TestMultiple(request);
            } else if (test_name == "binary") {
                return TestBinary(request);
            } else if (test_name == "concurrent") {
                return TestConcurrent(request);
            } else if (test_name == "nonblocking_read") {
                return TestNonblockingRead(request);
            } else if (test_name == "nonblocking_write") {
                return TestNonblockingWrite(request);
            }
            return "Unknown test";
        } catch (const std::exception& e) {
            return fmt::format("EXCEPTION: {}", e.what());
        }
    }

private:
    std::shared_ptr<websocket::WebSocketConnection> MakeWebSocketConnection(
        const server::http::HttpRequest& request,
        const std::string& uri = "/echo"
    ) const {
        const auto port = std::stoi(request.GetArg("port"));

        auto ws_response =
            client_.CreateRequest().url(fmt::format("ws://localhost:{}{}", port, uri)).PerformWebSocketHandshake();

        return ws_response.MakeWebSocketConnection();
    }

    std::string TestEcho(const server::http::HttpRequest& request) const {
        auto conn = MakeWebSocketConnection(request);

        conn->SendText("Hello WebSocket");
        websocket::Message msg;
        conn->Recv(msg);
        conn->Close(websocket::CloseStatus::kNormal);
        return (msg.data == "Hello WebSocket" && msg.is_text) ? "OK" : "FAIL";
    }

    std::string TestLarge(const server::http::HttpRequest& request) const {
        auto conn = MakeWebSocketConnection(request);

        std::string large_msg(50000, 'X');
        conn->SendText(large_msg);
        websocket::Message msg;
        conn->Recv(msg);
        conn->Close(websocket::CloseStatus::kNormal);
        return (msg.data == large_msg) ? "OK" : "FAIL";
    }

    std::string TestMultiple(const server::http::HttpRequest& request) const {
        auto conn = MakeWebSocketConnection(request);

        for (int i = 0; i < 50; ++i) {
            const auto text = fmt::format("msg{}", i);
            conn->SendText(text);
            websocket::Message msg;
            conn->Recv(msg);
            if (msg.data != text) {
                return "FAIL";
            }
        }
        conn->Close(websocket::CloseStatus::kNormal);
        return "OK";
    }

    std::string TestBinary(const server::http::HttpRequest& request) const {
        auto conn = MakeWebSocketConnection(request);

        std::vector<std::byte> data{std::byte{0x01}, std::byte{0xFF}};
        conn->SendBinary(data);
        websocket::Message msg;
        conn->Recv(msg);
        conn->Close(websocket::CloseStatus::kNormal);
        return (!msg.is_text && msg.data.size() == 2) ? "OK" : "FAIL";
    }

    std::string TestConcurrent(const server::http::HttpRequest& request) const {
        auto func = [](websocket::WebSocketConnection& conn, const std::string& msg) {
            conn.SendText(msg);
            websocket::Message resp;
            conn.Recv(resp);
            conn.Close(websocket::CloseStatus::kNormal);
            return resp.data == msg;
        };

        auto conn1 = MakeWebSocketConnection(request);
        auto conn2 = MakeWebSocketConnection(request);

        auto task1 = utils::Async("task1", func, std::ref(*conn1), std::string("msg1"));
        auto task2 = utils::Async("task2", func, std::ref(*conn2), std::string("msg2"));

        return (task1.Get() && task2.Get()) ? "OK" : "FAIL";
    }

    std::string TestNonblockingRead(const server::http::HttpRequest& request) const {
        auto conn0 = MakeWebSocketConnection(request);
        auto conn1 = MakeWebSocketConnection(request);

        conn0->SendText("msg0");
        conn1->SendText("msg1");

        auto wait_ctx = engine::MakeWaitAny(conn0->ReadAwaiter(), conn1->ReadAwaiter());

        websocket::Message msg;
        std::set<std::string> recv_messages{};
        while (!engine::current_task::ShouldCancel() && recv_messages.size() < 2) {
            auto result = wait_ctx.Wait();
            if (!result.has_value()) {
                return "WAIT CANCELED";
            }

            auto& conn = *result == 0 ? conn0 : conn1;

            conn->Recv(msg);
            recv_messages.insert(msg.data);
        }

        if (recv_messages != std::set<std::string>{"msg0", "msg1"}) {
            return "FAIL";
        }

        return "OK";
    }

    std::string TestNonblockingWrite(const server::http::HttpRequest& request) const {
        auto conn0 = MakeWebSocketConnection(request);
        auto conn1 = MakeWebSocketConnection(request);

        std::vector<std::string> messages{};
        for (int i = 0; i < 10; ++i) {
            auto wait_ctx = engine::MakeWaitAny(conn0->WriteAwaiter(), conn1->WriteAwaiter());
            auto result = wait_ctx.Wait();
            if (!result.has_value()) {
                return "WAIT CANCELED";
            }

            auto& conn = *result == 0 ? conn0 : conn1;
            conn->SendText(fmt::format("msg{}", i));
        }

        return "OK";
    }

    clients::http::Client& client_;
};

int main(int argc, char* argv[]) {
    const auto component_list =
        components::MinimalServerComponentList()
            .AppendComponentList(clients::http::ComponentList())
            .Append<clients::dns::Component>()
            .Append<WebSocketEcho>()
            .Append<TestClientHandler>()
            .Append<components::TestsuiteSupport>();
    return utils::DaemonMain(argc, argv, component_list);
}
