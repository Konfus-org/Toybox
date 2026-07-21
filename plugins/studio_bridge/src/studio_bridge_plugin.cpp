#include "studio_bridge_plugin.h"
#include "asset_rpc_handlers.h"
#include "collider_pass_ops.h"
#include "connection_rpc_handlers.h"
#include "data_plane_ops.h"
#include "draw_lane_ops.h"
#include "entity_rpc_handlers.h"
#include "game_mode_ops.h"
#include "gizmo_ops.h"
#include "gizmo_rpc_handlers.h"
#include "glass_ops.h"
#include "highlight_ops.h"
#include "hover_ops.h"
#include "input_ops.h"
#include "lifecycle_rpc_handlers.h"
#include "log_ops.h"
#include "log_rpc_handlers.h"
#include "physics_event_ops.h"
#include "physics_rpc_handlers.h"
#include "render_layers_rpc_handlers.h"
#include "sync_rpc_handlers.h"
#include "texture_rpc_handlers.h"
#include "rpc_registrar.h"
#include "selection_rpc_handlers.h"
#include "view_ops.h"
#include "view_rpc_handlers.h"
#include "world_rpc_handlers.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx::studio_bridge
{
    void StudioBridge::set_engine_paused(bool paused)
    {
        post_message<tbx::SetApplicationPausedRequest>(paused);
    }

    void StudioBridge::request_engine_step()
    {
        post_message<tbx::StepApplicationRequest>();
    }

    void StudioBridge::on_attach()
    {
        // The RPC router + host (published by the TcpRpc dependency) are bound by codegen before
        // on_attach. Share the host with the subsystems, then register the editor protocol's methods.
        _services.rpc_host = rpc_host;
        attach_log(_log, _services);
        register_builtin_handlers();

        // A studio-hosted engine opens stopped: pause simulation so the world renders its starting
        // state but does not advance until the editor enters play. (This replaces the old --editor
        // flag; the engine itself has no editor concept and otherwise runs immediately.)
        set_engine_paused(true);
    }

    void StudioBridge::on_detach()
    {
        // Remove our render passes from the engine Rendering service before this plugin unloads, so the
        // renderer never calls into the freed overlay captured here.
        unregister_gizmo_pass(_gizmos, _services);
        unregister_draw_lane_pass(_draw_lane, _services);
        unregister_collider_pass(_collider_pass, _services);
        unregister_editor_highlights(_highlights, _services);
        clear_all_glass(_glass, _services);
        stop_all_views(_views, _services);
        destroy_data_plane(_data_plane);
        detach_log(_log);
        // Clear our physics-event forwarders off any bound entities before this plugin unloads so a
        // later physics update never calls into freed bridge state.
        unbind_all_physics_events(_sync_events, _services);
        // Remove our handlers before this plugin unloads so the router never calls into freed memory.
        if (const auto active_router = router.lock())
            active_router->unregister_all(get_id());
        _services.app_name.clear();
        _had_client = false;
    }

    void StudioBridge::on_update(const tbx::DeltaTime& dt)
    {
        // The TcpRpc plugin owns the socket + drives the dispatch loop; the bridge only watches the
        // connection state to tear its views down when the editor leaves.
        const auto host = rpc_host.lock();
        const auto has_client = host && host->has_client();
        if (has_client != _had_client)
        {
            TBX_TRACE_INFO("StudioBridge: editor {}.", has_client ? "connected" : "disconnected");
            if (!has_client)
            {
                // The editor went away: tear down its views (and their glass passes), drop its
                // event subscriptions (its objects re-subscribe when they re-bind), and leave the
                // engine stopped at the restored world so it never lingers in a half-played state.
                stop_all_views(_views, _services);
                clear_all_glass(_glass, _services);
                release_all_view_slots(_data_plane);
                _sync_events.subscriptions.clear();
                set_playing(
                    _game_mode,
                    _sync_events,
                    _services,
                    [this](bool paused) { set_engine_paused(paused); },
                    false);
            }

            _had_client = has_client;
        }

        // Drain the data plane's input lanes into each view's ViewInput before any consumer below
        // reads it — the shared-memory fast path of the view.input notification (which remains the
        // fallback for views without a slot).
        drain_input_lanes(_data_plane, _views);

        // Prepare this frame for the engine's render: fly the focused editor cameras, hit-test/drag the
        // gizmo, feed the game's input system, and re-sync game views to the live game camera. Then fill
        // the overlay batches (gizmo handles + collider wireframes) and mirror every view's camera onto
        // the transient camera entity the bridge injects into its world. The engine renders those world
        // cameras after this update; the gizmo / collider / selection passes draw the overlays on top
        // (gated on the editor tag).
        update_editor_cameras(_services, _views, dt);
        update_gizmos(_gizmos, _selection, _services, _views, dt);
        update_hover(_hover, _picking, _services, _views);
        update_game_input(_services, _views, _game_mode.is_playing);
        sync_game_cameras(_views, _services);

        submit_gizmo_overlay(_gizmos, _selection, _services, _views);
        submit_draw_lane(_draw_lane, _data_plane, _services);
        submit_collider_wireframes(_collider_pass, _selection, _render_layers, _services);
        sync_camera_entities(_views, _services, _game_mode.is_playing);

        // Push each active view's entity projections + camera pose into the data plane — the
        // shared-memory fast path of the editor's overlay placement (the view.projectEntities RPC
        // remains the fallback for slotless views and RPC-only sessions).
        publish_view_lanes(_data_plane, _views, _services);

        // Mirror the game's mouse-lock mode out to the editor so its game panel can capture the
        // cursor.
        report_mouse_lock(_input, _services, _game_mode.is_playing);
    }

    void StudioBridge::on_recieve_message(tbx::Message& msg)
    {
        if (auto initialized_event = tbx::handle_message<tbx::ApplicationInitializedEvent>(msg))
        {
            auto& application = initialized_event->get().application;
            _services.app_name = application.get_name();
            _services.graphics_settings = std::cref(application.get_settings().graphics);
            auto& services = application.get_service_provider();
            _services.world_manager = services.try_get_service<tbx::WorldManager>();
            // The editor navigates with its own viewport cameras (in the bridge's render registry, not
            // the world), so the world-camera-driven view streamer would unload everything the editor
            // is looking at. Keep the whole world resident while editing instead.
            if (auto world_manager = _services.world_manager.lock())
                world_manager->set_streaming_enabled(false);
            _services.rendering = services.try_get_service<tbx::Rendering>();
            _services.graphics_backend = services.try_get_service<tbx::IGraphicsBackend>();
            _services.asset_manager = services.try_get_service<tbx::AssetManager>();
            _services.input_manager = services.try_get_service<tbx::InputManager>();
            _services.gizmos = services.try_get_service<tbx::Gizmos>();
            _services.physics = services.try_get_service<tbx::Physics>();
            _services.script_system = services.try_get_service<tbx::ScriptSystem>();

            // The rendering + backend + gizmo services are resolved now, so register the editor's
            // render passes (the gizmo overlay and the collider-wireframe pass, gated on the
            // editor.camera tag). They live on the engine Rendering service until on_detach removes them.
            register_gizmo_pass(_gizmos, _services);
            register_collider_pass(_collider_pass, _services);
            // The data plane's draw lane renders through its own overlay passes — the editor's
            // packed command buffers decoded into per-layer gizmo batches + textured sprites.
            register_draw_lane_pass(_draw_lane, _services, _textures);
            // The selection outline is another registered pass: it carries a post effect gated on
            // the editor.selected entity tag, scoped to editor cameras by the pass's tag gate.
            register_editor_highlights(_highlights, _services);

            // Physics events stream to the editor as sync.event raises while playing. Rather than a
            // service-wide observer, the bridge attaches its forwarders straight onto the subscribed
            // entities' Trigger/Collider callback lists when play starts (see physics_event_ops,
            // driven from set_playing / sync.subscribe); on_detach clears them.
        }
    }

    void StudioBridge::register_builtin_handlers()
    {
        // The editor protocol is split across the per-category `*_rpc_handlers` files; each is a free
        // function that registers its methods through the shared registrar. The handlers that need to
        // reach this plugin's message posting (pause / shutdown) get it as callbacks — everything else
        // is served by the plugin's state (through the per-domain ops) or a bridge subsystem the
        // function borrows by reference.
        const auto active_router = router.lock();
        if (!active_router)
        {
            TBX_TRACE_ERROR("StudioBridge: RPC router service is unavailable; no methods registered.");
            return;
        }

        const auto registrar = RpcRegistrar(*active_router, get_id());

        register_lifecycle_handlers(
            registrar,
            _services,
            _game_mode,
            _sync_events,
            _data_plane,
            [this](bool paused) { set_engine_paused(paused); },
            [this]() { request_engine_step(); },
            [this]() { post_message<tbx::ExitApplicationRequest>(); });
        register_world_handlers(registrar, _services, _views);
        register_asset_handlers(registrar, _services, _views);
        register_entity_handlers(registrar, _services, _views);
        register_connection_handlers(registrar, _services, _views);
        register_sync_handlers(registrar, _services, _views, _sync_events, _game_mode);
        register_physics_handlers(registrar, _services);
        register_view_handlers(registrar, _services, _views, _glass, _data_plane);
        register_selection_handlers(registrar, _picking, _selection, _services, _views, _gizmos);
        register_gizmo_handlers(registrar, _gizmos);
        register_render_layers_handlers(registrar, _render_layers, _services);
        register_texture_handlers(registrar, _textures);
        register_log_handlers(registrar);
    }
}
