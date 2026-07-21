#pragma once
#include "rpc_server.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/uuid.h"
#include <string>
#include <string_view>
#include <unordered_map>

namespace tbx::tcp_rpc
{
    /// @brief
    /// Purpose: Writes JSON-RPC replies for one request back to the client over the RPC server. A
    /// notification (null id) swallows replies, so notification handlers can reply harmlessly.
    /// @details
    /// Ownership: Borrows the server; stack-scoped per dispatched request. Thread Safety: Used on the
    /// main thread only.
    class ServerRpcResponder final : public tbx::RpcResponder
    {
      public:
        ServerRpcResponder(tbx::Json id, RpcServer& server);

      public:
        void result(const tbx::Json& reply) override;
        void error(int code, std::string_view message) override;
        bool is_notification() const override;

      private:
        tbx::Json _id;
        RpcServer& _server;
    };

    /// @brief
    /// Purpose: The RPC dispatch table. Maps method names to owner-tagged handlers and is published as
    /// tbx::IRpcRouter so any plugin can register methods; the TcpRpc plugin drives invoke() from its
    /// update loop and owns the lone strong reference (via the service provider).
    /// @details
    /// Ownership: Owns the handler closures. Thread Safety: Registration and dispatch happen on the
    /// main thread; not safe for concurrent use.
    class RpcRouter final : public tbx::IRpcRouter
    {
      public:
        RpcRouter() = default;

      public:
        tbx::Result register_method(
            std::string_view method,
            tbx::RpcHandler handler,
            tbx::Uuid owner) override;
        bool unregister_method(std::string_view method, tbx::Uuid owner) override;
        void unregister_all(tbx::Uuid owner) override;
        bool has_method(std::string_view method) const override;

      public:
        // Routes one request to its handler. Returns false when no handler is registered, so the
        // caller can emit a method-not-found error for a request that carries an id.
        bool invoke(const std::string& method, const tbx::Json& params, tbx::RpcResponder& responder);

      private:
        struct Entry
        {
            tbx::RpcHandler handler = {};
            tbx::Uuid owner = {};
        };

        std::unordered_map<std::string, Entry> _handlers = {};
    };
}
