#pragma once
#include "rpc_router.h"
#include "rpc_server.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/rpc_host.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <string>

namespace tbx::tcp_rpc
{
    /// @brief
    /// Purpose: Hosts the editor RPC transport — a loopback TCP server plus a dispatch router — and
    /// publishes them as tbx::IRpcHost + tbx::IRpcRouter so consumer plugins (e.g. StudioBridge)
    /// register methods and push notifications without owning any socket machinery. Loaded as a
    /// dependency of those consumers, so a shipped game never starts an RPC server.
    /// @details
    /// Ownership: The server + router service objects are provider-owned (codegen make_shared); this
    /// plugin holds weak views and drives the drain/dispatch loop each frame. Thread Safety:
    /// Main-thread only; the server's IO runs on its own thread and hands lines across a mutex.
    [[tbx::register_plugin(
        name = "TcpRpc",
        version = "0.1.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API TcpRpc final : public tbx::Plugin
    {
      public:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        // Published as services (codegen make_shared's them in the register phase; the provider owns
        // them, these are weak views). PUBLIC so the generated registration free function can reach
        // the fields directly.
        [[tbx::register(tbx::IRpcRouter)]]
        std::weak_ptr<RpcRouter> router = {};

        [[tbx::register(tbx::IRpcHost)]]
        std::weak_ptr<RpcServer> server = {};

      private:
        void handle_request_line(const std::string& line);

        uint16 _port = 0U;
    };
}
