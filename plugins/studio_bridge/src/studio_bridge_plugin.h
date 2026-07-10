#pragma once
#include "collider_pass_state.h"
#include "engine_services.h"
#include "game_mode_state.h"
#include "gizmo_layer_state.h"
#include "gizmo_state.h"
#include "input_state.h"
#include "log_state.h"
#include "picking_state.h"
#include "render_layers_state.h"
#include "selection_state.h"
#include "sync_event_state.h"
#include "view_state.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/interfaces/rpc_host.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/typedefs.h"
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
        StudioBridge() = default;
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
        // Registers every built-in RPC method (the editor protocol) on the router, delegating each
        // category to its `register_*_handlers` free function. Called from on_attach once the router is
        // bound; the editor's methods are owned by this plugin's id.
        void register_builtin_handlers();

      private:
        // Pauses/unpauses the engine simulation through the plugin's message posting; shared by the
        // lifecycle handlers and play-mode ops.
        void set_engine_paused(bool paused);

        // Queues a single simulation step through the plugin's message posting (the game view's
        // next-frame button); the engine advances one paused fixed tick on its next update.
        void request_engine_step();

      private:
        EngineServices _services = {};

        // The bridge's plain state, owned by value; the per-domain *_ops free functions carry the
        // behavior and receive exactly the state they touch.
        SelectionState _selection = {};
        RenderLayersState _render_layers = {};
        GameModeState _game_mode = {};
        SyncEventState _sync_events = {};
        LogState _log = {};
        PickingState _picking = {};
        ColliderPassState _collider_pass = {};
        InputState _input = {};
        GizmoLayerState _gizmo_layers = {};
        GizmoControllerState _gizmos = {};
        ViewState _views = {};

        bool _had_client = false;
    };
}
