#include "rpc_router.h"
#include "rpc_protocol.h"
#include <format>
#include <string>
#include <utility>

namespace tbx::windows_rpc
{
    ServerRpcResponder::ServerRpcResponder(tbx::Json id, RpcServer& server)
        : _id(std::move(id))
        , _server(server)
    {
    }

    void ServerRpcResponder::result(const tbx::Json& reply)
    {
        if (is_notification())
            return;
        _server.send_line(make_result_response(_id, reply));
    }

    void ServerRpcResponder::error(int code, std::string_view message)
    {
        if (is_notification())
            return;
        _server.send_line(make_error_response(_id, code, message));
    }

    bool ServerRpcResponder::is_notification() const
    {
        return _id.is_null();
    }

    tbx::Result RpcRouter::register_method(
        std::string_view method,
        tbx::RpcHandler handler,
        tbx::Uuid owner)
    {
        auto key = std::string(method);
        if (const auto it = _handlers.find(key); it != _handlers.end())
        {
            if (it->second.owner != owner)
                return tbx::Result(
                    false,
                    std::format(
                        "RPC method '{}' is already registered by another owner; "
                        "the new handler was ignored.",
                        key));

            // Same owner re-registering (hot-reload friendly): replace the handler in place.
            it->second.handler = std::move(handler);
            return tbx::Result::OK;
        }

        _handlers.emplace(std::move(key), Entry { std::move(handler), owner });
        return tbx::Result::OK;
    }

    bool RpcRouter::unregister_method(std::string_view method, tbx::Uuid owner)
    {
        const auto it = _handlers.find(std::string(method));
        if (it == _handlers.end() || it->second.owner != owner)
            return false;

        _handlers.erase(it);
        return true;
    }

    void RpcRouter::unregister_all(tbx::Uuid owner)
    {
        for (auto it = _handlers.begin(); it != _handlers.end();)
        {
            if (it->second.owner == owner)
                it = _handlers.erase(it);
            else
                ++it;
        }
    }

    bool RpcRouter::has_method(std::string_view method) const
    {
        return _handlers.contains(std::string(method));
    }

    bool RpcRouter::invoke(
        const std::string& method,
        const tbx::Json& params,
        tbx::RpcResponder& responder)
    {
        const auto it = _handlers.find(method);
        if (it == _handlers.end())
            return false;

        it->second.handler(params, responder);
        return true;
    }
}
