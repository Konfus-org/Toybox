#pragma once
#include "collider_pass.h"
#include "engine_services.h"
#include "entity_selection_handler.h"
#include "game_mode_manager.h"
#include "gizmo_controller.h"
#include "input_controller.h"
#include "log_bridge.h"
#include "selection.h"
#include "view_manager.h"
#include "world_manager.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/rpc_host.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <memory>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Bridges the engine to external tooling (Toybox Studio) over RPC. A thin orchestrator:
    /// it owns the shared engine-service handles and the bridge subsystems
    /// (views/input/gizmos/picking/play-mode/world-rpc/logging), and registers their RPC methods on
    /// the router published by the WindowsRPC plugin (the transport it depends on).
    /// @details
    /// Ownership: Owns every subsystem; borrows the RPC router + host as services. Thread Safety: Not
    /// thread-safe; requests are handled on the main thread.
    [[tbx::register_plugin(
        name = "StudioBridge",
        version = "0.1.0",
        category = tbx::PluginCategory::DEFAULT,
        dependencies = {"WindowsRPC"})]];
    class TBX_PLUGIN_API StudioBridge final : public tbx::Plugin
    {
      public:
        StudioBridge();
        ~StudioBridge() noexcept override = default;

      public:
        StudioBridge(const StudioBridge&) = delete;
        StudioBridge& operator=(const StudioBridge&) = delete;
        StudioBridge(StudioBridge&&) noexcept = delete;
        StudioBridge& operator=(StudioBridge&&) noexcept = delete;

      public:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      public:
        // The RPC services published by the WindowsRPC plugin, bound during the runtime-bind phase
        // (codegen). PUBLIC so the generated bind free function can reach the fields; held weak — the
        // service provider owns them. The dependency on WindowsRPC guarantees they resolve.
        [[tbx::inject]]
        std::weak_ptr<tbx::IRpcRouter> router = {};

        [[tbx::inject]]
        std::weak_ptr<tbx::IRpcHost> rpc_host = {};

      private:
        // Registers every built-in RPC method (the editor protocol) on the router. Called from
        // on_attach once the router is bound; the editor's methods are owned by this plugin's id.
        void register_builtin_handlers();
        tbx::Json handle_hello() const;

      private:
        EngineServices _services = {};
        Selection _selection = {};

        ViewManager _views;
        InputController _input;
        GizmoController _gizmos;
        ColliderPass _collider_pass;
        EntitySelectionHandler _selection_handler;
        GameModeManager _game_mode;
        WorldManager _world_manager;
        LogBridge _log;

        bool _had_client = false;
    };
}
