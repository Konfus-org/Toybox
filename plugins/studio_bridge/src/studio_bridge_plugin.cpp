#include "studio_bridge_plugin.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include <string>
#include <string_view>
#include <utility>

namespace tbx::studio_bridge
{
    // v2 adds the reflect.* namespace (per-property get/set/isDefault).
    constexpr int PROTOCOL_VERSION = 2;

    // App-specific JSON-RPC error code: a view RPC arrived but the target view/surface isn't available.
    // (The standard protocol codes live in tbx/interfaces/rpc_router.h.)
    constexpr int RPC_VIEW_UNAVAILABLE_CODE = -32000;

    StudioBridge::StudioBridge()
        : _views(_services)
        , _input(_services, _views)
        , _gizmos(_services, _views, _selection)
        , _collider_gizmos(_services, _selection)
        , _billboards(_services, _views)
        , _picking(_services, _views)
        , _play(
              _services,
              [this](bool paused)
              {
                  post_message<tbx::SetApplicationPausedRequest>(paused);
              })
        , _world_rpc(_services, _views)
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
                _play.set_playing(false);
            }

            _had_client = has_client;
        }

        // Apply this frame's forwarded input: fly the focused editor cameras and feed the game's
        // input system, then re-sync game views to the live game camera, all before rendering reads
        // their transforms. The view manager then renders its own view cameras into their textures,
        // submitting the gizmo overlay first.
        _input.update_editor_cameras(dt);
        _gizmos.update(dt);
        _input.update_game_input(_play.is_playing());
        _views.sync_game_views();
        _views.render_views(
            dt,
            [this](const tbx::CameraView* focused_camera, const GizmoState* focused_gizmo)
            {
                // Collider/trigger wireframes (world-space, every editor view) plus the focused
                // view's transform handles make up this frame's gizmo overlay.
                _collider_gizmos.submit();
                _gizmos.submit_overlay(focused_camera, focused_gizmo);
            });

        // Push this frame's entity screen positions to the editor's billboard overlay (name labels +
        // viewport-icon stacks), one message per editor view. Editor-only; cheap no-op without a client.
        _billboards.publish();

        // Mirror the game's mouse-lock mode out to the editor so its game panel can capture the
        // cursor.
        _input.report_mouse_lock(_play.is_playing());
    }

    void StudioBridge::on_recieve_message(tbx::Message& msg)
    {
        if (auto initialized_event = tbx::handle_message<tbx::ApplicationInitializedEvent>(msg))
        {
            auto& application = initialized_event->get().application;
            _services.app_name = application.get_name();
            _services.graphics_settings = &application.get_settings().graphics;
            auto& services = application.get_service_provider();
            _services.world_manager = services.try_get_service<tbx::WorldManager>();
            _services.rendering = services.try_get_service<tbx::Rendering>();
            _services.asset_manager = services.try_get_service<tbx::AssetManager>();
            _services.input_manager = services.try_get_service<tbx::InputManager>();
            _services.gizmos = services.try_get_service<tbx::Gizmos>();
            _services.scripting_registry = services.try_get_service<tbx::ScriptingRegistry>();
            _services.physics = services.try_get_service<tbx::Physics>();
            _services.script_system = services.try_get_service<tbx::ScriptSystem>();
        }
    }

    tbx::Json StudioBridge::handle_hello() const
    {
        auto result = tbx::Json::object();
        result["protocolVersion"] = PROTOCOL_VERSION;
        result["engine"] = "Toybox";
        result["app"] = _services.app_name;
        return result;
    }

    void StudioBridge::register_builtin_handlers()
    {
        const auto active_router = router.lock();
        if (!active_router)
        {
            TBX_TRACE_ERROR("StudioBridge: RPC router service is unavailable; no methods registered.");
            return;
        }

        const auto owner = get_id();
        const auto add = [&active_router, owner](std::string_view method, tbx::RpcHandler handler)
        {
            // A collision means another plugin already owns the method; the router keeps its handler
            // and ours is dropped, silently breaking that part of the editor protocol — surface it.
            if (const tbx::Result registered =
                    active_router->register_method(method, std::move(handler), owner);
                !registered)
                TBX_TRACE_ERROR(
                    "StudioBridge: could not register RPC method '{}': {}",
                    method,
                    registered.get_report());
        };

        // --- Handshake / lifecycle ---
        add(
            "editor.hello",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(handle_hello());
            });
        add(
            "engine.ping",
            [](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(tbx::Json::object());
            });
        add(
            "engine.setPaused",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                post_message<tbx::SetApplicationPausedRequest>(params.value("isPaused", false));
                r.result(tbx::Json::object());
            });
        add(
            "engine.setPlaying",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                _play.set_playing(params.value("isPlaying", false));
                r.result(tbx::Json::object());
            });
        add(
            "engine.shutdown",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(tbx::Json::object());
                TBX_TRACE_INFO("StudioBridge: shutdown requested by the editor.");
                post_message<tbx::ExitApplicationRequest>();
            });

        // --- World / entity / asset description + edits (WorldRpc) ---
        add(
            "world.describe",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(_world_rpc.describe_world());
            });
        add(
            "world.save",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.save_world());
            });
        add(
            "world.open",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Opens a world/chunk asset as the active editing world (replacing the current one).
                r.respond(_world_rpc.open_world(params));
            });
        add(
            "asset.save",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.save_asset(params));
            });
        add(
            "editor.listAssets",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(_world_rpc.list_assets());
            });
        add(
            "app.describeSettings",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(_world_rpc.describe_settings());
            });
        add(
            "asset.describe",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _world_rpc.describe_asset(params, reply);
                r.respond(result, reply);
            });
        add(
            "editor.listComponentTypes",
            [this](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(_world_rpc.list_component_types());
            });
        add(
            "editor.modelSlots",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _world_rpc.model_slots(params, reply);
                r.respond(result, reply);
            });
        add(
            "entity.setComponent",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.apply_component(params));
            });
        add(
            "entity.addComponent",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.add_component(params));
            });
        add(
            "entity.removeComponent",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.remove_component(params));
            });
        add(
            "entity.addScript",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.add_script(params));
            });
        add(
            "entity.describe",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _world_rpc.describe_entity(params, reply);
                r.respond(result, reply);
            });
        add(
            "entity.create",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _world_rpc.create_entity(params, reply);
                r.respond(result, reply);
            });
        add(
            "entity.destroy",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.destroy_entity(params));
            });
        add(
            "entity.setName",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.set_entity_name(params));
            });
        add(
            "entity.setGlobal",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.set_entity_global(params));
            });
        add(
            "entity.setEnabled",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.set_entity_enabled(params));
            });
        add(
            "entity.move",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.move_entity(params));
            });

        // --- Per-property reflection (WorldRpc) ---
        add(
            "reflect.get",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto node = tbx::Json::object();
                const auto result = _world_rpc.reflect_get(params, node);
                r.respond(result, node);
            });
        add(
            "reflect.set",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.reflect_set(params));
            });
        add(
            "reflect.reset",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(_world_rpc.reflect_reset(params));
            });
        add(
            "reflect.isDefault",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto is_default = false;
                const auto result = _world_rpc.reflect_is_default(params, is_default);
                if (result)
                {
                    auto reply = tbx::Json::object();
                    reply["isDefault"] = is_default;
                    r.result(reply);
                }
                else
                {
                    r.respond(result);
                }
            });

        // --- Views (ViewManager) ---
        add(
            "view.start",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                const auto kind_token = params.value("kind", std::string());
                const auto kind = kind_token == "game"  ? ViewKind::Game
                                  : kind_token == "asset" ? ViewKind::AssetPreview
                                                          : ViewKind::Editor;
                // Only an asset-preview view uses the asset id; it selects the asset loaded into the
                // view's isolated preview world.
                const auto asset_id = params.value("assetId", 0U);
                auto view_name = std::string();
                auto view_result = _views.start_view(kind, asset_id, view_name);
                if (view_result)
                {
                    auto view_info = tbx::Json::object();
                    view_info["name"] = view_name;
                    view_info["format"] = "bgra8";
                    r.result(view_info);
                }
                else
                {
                    r.error(RPC_VIEW_UNAVAILABLE_CODE, view_result.get_report());
                }
            });
        add(
            "view.stop",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                const auto name = params.value("name", std::string());
                if (name.empty())
                    _views.stop_all_views();
                else
                    _views.stop_view(name);
                r.result(tbx::Json::object());
            });
        add(
            "view.input",
            [this](const tbx::Json& params, tbx::RpcResponder&)
            {
                // High-frequency notification from the focused editor viewport; no response.
                _views.apply_view_input(params);
            });
        add(
            "view.setPreviewOption",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Rebuilds an asset-preview view with a different mesh/material option (the editor's
                // preview picker).
                const auto name = params.value("view", std::string());
                const auto option = params.value("option", std::string());
                r.respond(_views.set_preview_option(name, option));
            });
        add(
            "view.setPreviewSkybox",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Rebuilds an asset-preview view with a different background sky (the editor's skybox
                // picker).
                const auto name = params.value("view", std::string());
                const auto skybox = params.value("skybox", std::string());
                r.respond(_views.set_preview_skybox(name, skybox));
            });

        // --- Picking + selection (PickService / Selection) ---
        add(
            "view.pick",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _picking.pick(params, reply);
                r.respond(result, reply);
            });
        add(
            "view.pickRect",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto reply = tbx::Json::object();
                const auto result = _picking.pick_rect(params, reply);
                r.respond(result, reply);
            });
        add(
            "view.queryOcclusion",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Which of the given entities are hidden behind geometry from a view's camera (billboard
                // overlay visibility), batched into one scene pass.
                auto reply = tbx::Json::object();
                const auto result = _picking.query_occlusion(params, reply);
                r.respond(result, reply);
            });
        add(
            "view.setSelection",
            [this](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor whenever the selection changes; no response. The
                // selection is shown as an outline by the engine's tag-gated selection-outline post
                // effect, so mark the newly selected entities with the runtime SELECTION_TAG and clear
                // it from the previously selected ones (a no-op on stale/absent ids). An id may live in
                // the active world or in an asset-preview view's world, so resolve each id's world.
                const auto world_of = [this](const tbx::Uuid& id) -> std::shared_ptr<tbx::World>
                {
                    if (auto world = _services.active_world(); world && world->has(id))
                        return world;
                    return _views.find_preview_world_with(id);
                };

                for (const auto& id : _selection.ids())
                    if (auto world = world_of(id))
                        world->get(id).remove_tag(SELECTION_TAG);
                _selection.set_from_params(params);
                for (const auto& id : _selection.ids())
                    if (auto world = world_of(id))
                        world->get(id).add_tag(SELECTION_TAG, /*serialized*/ false);
            });

        // --- Gizmo tool (GizmoController) ---
        add(
            "view.setGizmo",
            [this](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor's transform-tool toolbar; no response.
                _gizmos.set_mode(params);
            });

        // --- Logging (LogBridge) ---
        add(
            "editor.log",
            [this](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor: write its line into the engine's unified log. No
                // response.
                _log.write_editor_log(params);
            });
        add(
            "engine.setLogColors",
            [this](const tbx::Json& params, tbx::RpcResponder& r)
            {
                _log.set_log_colors(params);
                r.result(tbx::Json::object());
            });
    }
}
