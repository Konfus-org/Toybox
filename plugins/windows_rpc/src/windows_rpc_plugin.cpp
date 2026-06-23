#include "windows_rpc_plugin.h"
#include "rpc_protocol.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/debugging/macros.h"
#include <cstdlib>
#include <string>

namespace tbx::windows_rpc
{
    constexpr uint16 DEFAULT_RPC_PORT = 17890U;

    static uint16 read_port_from_env()
    {
        char* port_text = nullptr;
        auto port = static_cast<int>(DEFAULT_RPC_PORT);
        if (_dupenv_s(&port_text, nullptr, "TBX_STUDIO_RPC_PORT") == 0 && port_text != nullptr)
        {
            const auto parsed = std::atoi(port_text);
            if (parsed > 0 && parsed <= 65535)
                port = parsed;

            std::free(port_text);
        }

        return static_cast<uint16>(port);
    }

    void WindowsRpc::on_attach()
    {
        _port = read_port_from_env();
        const auto active_server = server.lock();
        if (!active_server)
        {
            TBX_TRACE_ERROR("WindowsRPC: RPC host service is unavailable; not starting.");
            return;
        }

        if (auto result = active_server->start(_port); !result)
        {
            TBX_TRACE_ERROR(
                "WindowsRPC: failed to start the RPC server on port {}: {}",
                _port,
                result.get_report());
            return;
        }

        TBX_TRACE_INFO("WindowsRPC: listening on 127.0.0.1:{}", _port);
    }

    void WindowsRpc::on_detach()
    {
        if (const auto active_server = server.lock())
            active_server->stop();
    }

    void WindowsRpc::on_update(const tbx::DeltaTime&)
    {
        const auto active_server = server.lock();
        if (!active_server)
            return;

        for (const auto& line : active_server->take_received_lines())
            handle_request_line(line);
    }

    void WindowsRpc::handle_request_line(const std::string& line)
    {
        const auto active_server = server.lock();
        if (!active_server)
            return;

        const auto message = try_parse_message(line);
        if (!message)
        {
            active_server->send_line(make_error_response(
                tbx::Json(),
                tbx::RPC_PARSE_ERROR_CODE,
                "Failed to parse request."));
            return;
        }

        const auto& request = *message;
        const auto id = request.value("id", tbx::Json());
        const auto method = request.value("method", std::string());
        const auto params = request.value("params", tbx::Json::object());

        const auto active_router = router.lock();
        ServerRpcResponder responder(id, *active_server);
        if (active_router && active_router->invoke(method, params, responder))
            return;

        // No handler claimed the method. Requests (with an id) get a method-not-found error;
        // notifications are silently ignored.
        if (!id.is_null())
        {
            active_server->send_line(make_error_response(
                id,
                tbx::RPC_METHOD_NOT_FOUND_CODE,
                "Unknown method: " + method));
        }
    }
}
