#include "studio_bridge_plugin.h"
#include "asset_rpc_handlers.h"
#include "entity_rpc_handlers.h"
#include "gizmo_rpc_handlers.h"
#include "lifecycle_rpc_handlers.h"
#include "log_rpc_handlers.h"
#include "sync_rpc_handlers.h"
#include "rpc_registrar.h"
#include "selection_rpc_handlers.h"
#include "view_rpc_handlers.h"
#include "world_rpc_handlers.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"

namespace tbx::studio_bridge
{
    StudioBridge::StudioBridge()
        : _views(_services)
        , _input(_services, _views)
        , _gizmos(_services, _views, _selection)
        , _collider_pass(_services, _selection)
        , _selection_handler(_services, _views)
        , _game_mode(
              _services,
              [this](bool paused)
              {
                  post_message<tbx::SetApplicationPausedRequest>(paused);
              })
        , _asset_ops(_services)
        , _world_manager(_services, _views)
        , _sync_router(_world_manager, _asset_ops)
        , _log(_services)
    {
    }

    void StudioBridge::on_attach()
    {
        // The RPC router + host (published by the WindowsRPC dependency) are bound by codegen before
        // on_attach. Share the host with the subsystems, then register the editor protocol's methods.
        _services.rpc_host = rpc_host;
        _log.attach();
        register_builtin_handlers();

        // A studio-hosted engine opens stopped: pause simulation so the world renders its starting
        // state but does not advance until the editor enters play. (This replaces the old --editor
        // flag; the engine itself has no editor concept and otherwise runs immediately.)
        post_message<tbx::SetApplicationPausedRequest>(true);
    }

    void StudioBridge::on_detach()
    {
        // Remove our render passes from the engine Rendering service before this plugin unloads, so the
        // renderer never calls into the freed overlay captured here.
        _gizmos.unregister_pass();
        _collider_pass.unregister_pass();
        _views.stop_all_views();
        _log.detach();
        // Remove our handlers before this plugin unloads so the router never calls into freed memory.
        if (const auto active_router = router.lock())
            active_router->unregister_all(get_id());
        _services.app_name.clear();
        _had_client = false;
    }

    void StudioBridge::on_update(const tbx::DeltaTime& dt)
    {
        // The WindowsRPC plugin owns the socket + drives the dispatch loop; the bridge only watches the
        // connection state to tear its views down when the editor leaves.
        const auto host = rpc_host.lock();
        const auto has_client = host && host->has_client();
        if (has_client != _had_client)
        {
            TBX_TRACE_INFO("StudioBridge: editor {}.", has_client ? "connected" : "disconnected");
            if (!has_client)
            {
                // The editor went away: tear down its views and leave the engine stopped at the
                // restored world so it never lingers in a half-played state.
                _views.stop_all_views();
                _game_mode.set_playing(false);
            }

            _had_client = has_client;
        }

        // Prepare this frame for the engine's render: fly the focused editor cameras, hit-test/drag the
        // gizmo, feed the game's input system, and re-sync game views to the live game camera. Then fill
        // the overlay batches (gizmo handles + collider wireframes) and push every view's camera to the
        // engine's external-camera registry. The engine renders the external cameras after this update;
        // the gizmo / collider / selection passes draw the overlays on top (gated on the editor tag).
        _input.update_editor_cameras(dt);
        _gizmos.update(dt);
        _input.update_game_input(_game_mode.is_playing());
        _views.sync_game_cameras();

        _gizmos.submit_overlay();
        _collider_pass.submit();
        _views.push_external_cameras();

        // The editor's billboard overlay positions are pulled, not pushed: the editor polls
        // view.projectEntities on its own cadence, so there is nothing to send here per frame.

        // Mirror the game's mouse-lock mode out to the editor so its game panel can capture the
        // cursor.
        _input.report_mouse_lock(_game_mode.is_playing());
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
            _services.scripting_registry = services.try_get_service<tbx::ScriptingRegistry>();
            _services.physics = services.try_get_service<tbx::Physics>();
            _services.script_system = services.try_get_service<tbx::ScriptSystem>();

            // The rendering + backend + gizmo services are resolved now, so register the editor's
            // render passes (the gizmo overlay and the collider-wireframe pass, gated on the
            // editor.camera tag). They live on the engine Rendering service until on_detach removes them.
            _gizmos.register_pass();
            _collider_pass.register_pass();
        }
    }

    void StudioBridge::register_builtin_handlers()
    {
        // The editor protocol is split across the per-category `*_rpc_handlers` files; each is a free
        // function that registers its methods through the shared registrar. The handlers that need to
        // reach this plugin's message posting (pause / shutdown) get it as callbacks — everything else
        // is served by a bridge subsystem the function borrows by reference.
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
            [this](bool paused) { post_message<tbx::SetApplicationPausedRequest>(paused); },
            [this]() { post_message<tbx::ExitApplicationRequest>(); });
        register_world_handlers(registrar, _world_manager);
        register_asset_handlers(registrar, _asset_ops, _world_manager);
        register_entity_handlers(registrar, _world_manager);
        register_sync_handlers(registrar, _sync_router, _world_manager);
        register_view_handlers(registrar, _views);
        register_selection_handlers(registrar, _selection_handler, _selection, _services, _views);
        register_gizmo_handlers(registrar, _gizmos);
        register_log_handlers(registrar, _log);
    }
}
