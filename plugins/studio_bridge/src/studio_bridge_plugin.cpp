#include "studio_bridge_plugin.h"
#include "rpc_protocol.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/assets/describe.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity_serialization.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/color.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    constexpr uint16 DEFAULT_RPC_PORT = 17890U;
    // v2 adds the reflect.* namespace (per-property get/set/isDefault).
    constexpr int PROTOCOL_VERSION = 2;

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

    static int hex_nibble(char character)
    {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return (character - 'a') + 10;
        if (character >= 'A' && character <= 'F')
            return (character - 'A') + 10;

        return -1;
    }

    static float hex_channel(const std::string& hex, size offset)
    {
        if (offset + 1 >= hex.size())
            return 0.0F;

        const auto high = hex_nibble(hex[offset]);
        const auto low = hex_nibble(hex[offset + 1]);
        if (high < 0 || low < 0)
            return 0.0F;

        return static_cast<float>((high * 16) + low) / 255.0F;
    }

    // Parses "#RRGGBB" (or "RRGGBB"); anything malformed falls back to white so a bad color never
    // silences the console.
    static tbx::Color parse_hex_color(const std::string& hex)
    {
        const auto body = (!hex.empty() && hex.front() == '#') ? hex.substr(1) : hex;
        if (body.size() < 6)
            return tbx::Color(1.0F, 1.0F, 1.0F, 1.0F);

        return tbx::Color(hex_channel(body, 0), hex_channel(body, 2), hex_channel(body, 4), 1.0F);
    }

    static std::string_view to_log_level_name(tbx::LogLevel level)
    {
        switch (level)
        {
            case tbx::LogLevel::INFO:
                return "info";
            case tbx::LogLevel::WARNING:
                return "warning";
            case tbx::LogLevel::ERROR:
                return "error";
            case tbx::LogLevel::CRITICAL:
                return "critical";
        }

        return "info";
    }

    static std::string_view to_lock_mode_name(tbx::MouseLockMode mode)
    {
        switch (mode)
        {
            case tbx::MouseLockMode::RELATIVE:
                return "relative";
            case tbx::MouseLockMode::INPUT_GRABBED:
                return "grabbed";
            case tbx::MouseLockMode::UNLOCKED:
                return "unlocked";
        }

        return "unlocked";
    }

    // Builds a rotation whose -Z (camera forward) points along `forward`, with the horizon kept level
    // against `world_up`. Used to aim the editor camera at the scene.
    static tbx::Quat look_rotation(const glm::vec3& forward, const glm::vec3& world_up)
    {
        const auto f = glm::normalize(forward);
        // Near-vertical looks have no stable horizon against world up; fall back to a different axis.
        auto up_reference = world_up;
        if (std::abs(glm::dot(f, up_reference)) > 0.999F)
            up_reference = glm::vec3(0.0F, 0.0F, 1.0F);

        const auto right = glm::normalize(glm::cross(f, up_reference));
        const auto up = glm::cross(right, f);
        return glm::normalize(glm::quat_cast(glm::mat3(right, up, -f)));
    }

    // Average world position of the scene's renderable (static-mesh) entities. Gives the editor camera
    // something meaningful to face when it opens. Returns false when the scene has no renderable
    // geometry. (View cameras live in a separate registry, so the world holds only the user's scene.)
    static bool compute_scene_focus(tbx::World& world, glm::vec3& out_focus)
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (auto& entity : world.get_with<tbx::StaticMesh>())
        {
            if (!entity.has_component<tbx::Transform>())
                continue;

            sum += entity.get_component<tbx::Transform>().to_world_space(entity).position;
            ++count;
        }

        if (count == 0)
            return false;

        out_focus = sum / static_cast<float>(count);
        return true;
    }

    void StudioBridge::on_attach()
    {
        _port = read_port_from_env();
        const auto port = _port;
        auto result = _server.start(port);
        if (!result)
        {
            TBX_TRACE_ERROR(
                "StudioBridge: failed to start the RPC server on port {}: {}",
                port,
                result.get_report());
            return;
        }

        TBX_TRACE_INFO("StudioBridge: listening on 127.0.0.1:{}", port);
        _log_listener_id = tbx::Log::get_instance().add_listener(
            [this](tbx::LogLevel level, const std::string& message)
            {
                forward_log(level, message);
            });

        // A studio-hosted engine opens stopped: pause simulation so the world renders its starting
        // state but does not advance until the editor enters play. (This replaces the old --editor
        // flag; the engine itself has no editor concept and otherwise runs immediately.)
        post_message<tbx::SetApplicationPausedRequest>(true);
    }

    void StudioBridge::on_detach()
    {
        stop_all_views();
        if (_log_listener_id != 0U)
        {
            tbx::Log::get_instance().remove_listener(_log_listener_id);
            _log_listener_id = 0U;
        }

        _server.stop();
        _app_name.clear();
        _had_client = false;
    }

    void StudioBridge::on_update(const tbx::DeltaTime& dt)
    {
        const auto has_client = _server.has_client();
        if (has_client != _had_client)
        {
            TBX_TRACE_INFO(
                "StudioBridge: editor {}.",
                has_client ? "connected" : "disconnected");
            if (!has_client)
            {
                // The editor went away: tear down its views and leave the engine stopped at the
                // restored world so it never lingers in a half-played state.
                stop_all_views();
                set_play_state(false);
            }

            _had_client = has_client;
        }

        for (const auto& line : _server.take_received_lines())
            handle_request_line(line);

        // Apply this frame's forwarded input: fly the focused editor cameras and feed the game's input
        // system, then re-sync game views to the live game camera, all before rendering reads their
        // transforms. The plugin then renders its own view cameras into their textures.
        update_editor_cameras(dt);
        update_game_input();
        sync_game_views();
        render_views(dt);

        // Mirror the game's mouse-lock mode out to the editor so its game panel can capture the cursor.
        report_mouse_lock();
    }

    void StudioBridge::on_recieve_message(tbx::Message& msg)
    {
        if (auto initialized_event = tbx::handle_message<tbx::ApplicationInitializedEvent>(msg))
        {
            auto& application = initialized_event->get().application;
            _app_name = application.get_name();
            _graphics_settings = &application.get_settings().graphics;
            auto& services = application.get_service_provider();
            _world_manager = services.try_get_service<tbx::WorldManager>();
            _rendering = services.try_get_service<tbx::Rendering>();
            _asset_manager = services.try_get_service<tbx::AssetManager>();
            _input_manager = services.try_get_service<tbx::IInputManager>();
        }
    }

    // Set on the thread that is writing an editor-originated log so the listener below does not
    // echo that line straight back to the editor (which already displayed it locally). Thread-local
    // because the log listener runs synchronously on whatever thread called into tbx::Log.
    static thread_local bool t_suppress_log_forward = false;

    void StudioBridge::forward_log(tbx::LogLevel level, const std::string& message)
    {
        if (t_suppress_log_forward || !_server.has_client())
            return;

        auto params = tbx::Json::object();
        params["level"] = to_log_level_name(level);
        params["message"] = message;
        _server.send_line(make_notification("engine.log", params));
    }

    void StudioBridge::write_editor_log(const tbx::Json& params)
    {
        const auto message = params.value("message", std::string());
        if (message.empty())
            return;

        const auto level_name = params.value("level", std::string("info"));
        auto level = tbx::LogLevel::INFO;
        if (level_name == "warning")
            level = tbx::LogLevel::WARNING;
        else if (level_name == "error")
            level = tbx::LogLevel::ERROR;
        else if (level_name == "critical")
            level = tbx::LogLevel::CRITICAL;

        // Route editor lines through the engine's normal logging (file + console + listeners) but
        // skip the RPC echo so the editor doesn't show its own line twice. Tag them "[Studio]" via a
        // category scope (no source file — the editor's own console keeps the file:line detail).
        t_suppress_log_forward = true;
        {
            TBX_LOG_CATEGORY_SCOPE("Studio");
            tbx::Log::get_instance().write_internal(level, nullptr, 0, message);
        }
        t_suppress_log_forward = false;
    }

    void StudioBridge::handle_set_log_colors(const tbx::Json& params)
    {
        auto& log = tbx::Log::get_instance();
        if (params.contains("info"))
            log.set_color(
                tbx::LogLevel::INFO,
                parse_hex_color(params.value("info", std::string())));

        if (params.contains("warning"))
            log.set_color(
                tbx::LogLevel::WARNING,
                parse_hex_color(params.value("warning", std::string())));

        if (params.contains("error"))
        {
            const auto error_color = parse_hex_color(params.value("error", std::string()));
            log.set_color(tbx::LogLevel::ERROR, error_color);
            log.set_color(tbx::LogLevel::CRITICAL, error_color);
        }
    }

    void StudioBridge::handle_request_line(const std::string& line)
    {
        const auto message = try_parse_message(line);
        if (!message)
        {
            _server.send_line(make_error_response(
                tbx::Json(),
                JSON_RPC_PARSE_ERROR_CODE,
                "Failed to parse request."));
            return;
        }

        const auto& request = *message;
        const auto id = request.value("id", tbx::Json());
        const auto method = request.value("method", std::string());

        if (method == "editor.hello")
        {
            _server.send_line(make_result_response(id, handle_hello()));
        }
        else if (method == "engine.ping")
        {
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "world.describe")
        {
            _server.send_line(make_result_response(id, handle_describe_world()));
        }
        else if (method == "world.save")
        {
            const auto result = save_world();
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "asset.save")
        {
            const auto result = save_asset(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "editor.listAssets")
        {
            _server.send_line(make_result_response(id, handle_list_assets()));
        }
        else if (method == "app.describeSettings")
        {
            _server.send_line(make_result_response(id, describe_settings()));
        }
        else if (method == "asset.describe")
        {
            auto reply = tbx::Json::object();
            const auto result = describe_asset(request.value("params", tbx::Json::object()), reply);
            if (result)
                _server.send_line(make_result_response(id, reply));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.setComponent")
        {
            const auto result = apply_component(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.describe")
        {
            auto reply = tbx::Json::object();
            const auto result = describe_entity(request.value("params", tbx::Json::object()), reply);
            if (result)
                _server.send_line(make_result_response(id, reply));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.create")
        {
            auto reply = tbx::Json::object();
            const auto result = create_entity(request.value("params", tbx::Json::object()), reply);
            if (result)
                _server.send_line(make_result_response(id, reply));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.destroy")
        {
            const auto result = destroy_entity(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.setName")
        {
            const auto result = set_entity_name(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.setGlobal")
        {
            const auto result = set_entity_global(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "entity.move")
        {
            const auto result = move_entity(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "reflect.get")
        {
            auto node = tbx::Json::object();
            const auto result = reflect_get(request.value("params", tbx::Json::object()), node);
            if (result)
                _server.send_line(make_result_response(id, node));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "reflect.set")
        {
            const auto result = reflect_set(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "reflect.reset")
        {
            const auto result = reflect_reset(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "reflect.isDefault")
        {
            auto is_default = false;
            const auto result =
                reflect_is_default(request.value("params", tbx::Json::object()), is_default);
            if (result)
            {
                auto reply = tbx::Json::object();
                reply["isDefault"] = is_default;
                _server.send_line(make_result_response(id, reply));
            }
            else
            {
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
            }
        }
        else if (method == "view.start")
        {
            const auto params = request.value("params", tbx::Json::object());
            const auto is_game = params.value("kind", std::string()) == "game";
            auto view_name = std::string();
            auto view_result = start_view(is_game, view_name);
            if (view_result)
            {
                auto view_info = tbx::Json::object();
                view_info["name"] = view_name;
                view_info["format"] = "bgra8";
                _server.send_line(make_result_response(id, view_info));
            }
            else
            {
                _server.send_line(make_error_response(
                    id,
                    JSON_RPC_VIEW_UNAVAILABLE_CODE,
                    view_result.get_report()));
            }
        }
        else if (method == "view.stop")
        {
            const auto params = request.value("params", tbx::Json::object());
            const auto name = params.value("name", std::string());
            if (name.empty())
                stop_all_views();
            else
                stop_view(name);
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "view.input")
        {
            // High-frequency notification from the focused editor viewport; no response.
            handle_view_input(request.value("params", tbx::Json::object()));
        }
        else if (method == "engine.setPaused")
        {
            const auto params = request.value("params", tbx::Json::object());
            const auto is_paused = params.value("isPaused", false);
            post_message<tbx::SetApplicationPausedRequest>(is_paused);
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "engine.setPlaying")
        {
            const auto params = request.value("params", tbx::Json::object());
            set_play_state(params.value("isPlaying", false));
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "editor.log")
        {
            // Notification from the editor: write its line into the engine's unified log. No
            // response.
            write_editor_log(request.value("params", tbx::Json::object()));
        }
        else if (method == "engine.setLogColors")
        {
            handle_set_log_colors(request.value("params", tbx::Json::object()));
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "engine.shutdown")
        {
            _server.send_line(make_result_response(id, tbx::Json::object()));
            TBX_TRACE_INFO("StudioBridge: shutdown requested by the editor.");
            post_message<tbx::ExitApplicationRequest>();
        }
        else if (!id.is_null())
        {
            _server.send_line(make_error_response(
                id,
                JSON_RPC_METHOD_NOT_FOUND_CODE,
                "Unknown method: " + method));
        }
    }

    tbx::Json StudioBridge::handle_hello() const
    {
        auto result = tbx::Json::object();
        result["protocolVersion"] = PROTOCOL_VERSION;
        result["engine"] = "Toybox";
        result["app"] = _app_name;
        return result;
    }

    Result StudioBridge::start_view(bool is_game, std::string& out_name)
    {
        auto rendering = _rendering.lock();
        if (!rendering)
            return Result(false, "Rendering service is unavailable.");
        if (_graphics_settings == nullptr)
            return Result(false, "Graphics settings are unavailable.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world to view.");

        // A fresh index per view keeps every render texture (and its shared surface) distinct, so
        // the editor can stream and tear down each viewport independently.
        const auto index = _next_view_index++;
        auto view = std::make_unique<ViewStream>();
        view->name = std::format("ToyboxStudioFrame_{}_{}", _port, index);
        view->is_game = is_game;

        view->texture = tbx::RenderTexture(std::format("StudioView_{}_{}", _port, index));
        // Show the game at its real size: the view renders at the configured graphics resolution.
        view->texture.size = _graphics_settings->resolution;
        view->camera_id = create_view_camera(*world, view->texture, is_game);

        out_name = view->name;
        {
            auto lock = std::lock_guard(_views_mutex);
            _views.push_back(std::move(view));
        }

        refresh_present_callback();
        TBX_TRACE_INFO(
            "StudioBridge: {} view started ('{}').",
            is_game ? "game" : "editor",
            out_name);
        return Result::OK;
    }

    void StudioBridge::stop_view(const std::string& name)
    {
        std::unique_ptr<ViewStream> removed;
        {
            auto lock = std::lock_guard(_views_mutex);
            const auto it = std::find_if(
                _views.begin(),
                _views.end(),
                [&name](const std::unique_ptr<ViewStream>& view) { return view->name == name; });
            if (it == _views.end())
                return;

            removed = std::move(*it);
            _views.erase(it);
            // GL/D3D teardown is only safe on the render lane, so hand the surface to the next
            // present callback rather than freeing it here on the main thread.
            _pending_shared_destroys.push_back(removed->texture);
        }

        // Tear the camera down outside the lock; the render lane never touches a view once it is
        // out of _views.
        destroy_view_camera(removed->camera_id);
        refresh_present_callback();
        TBX_TRACE_INFO("StudioBridge: view stopped ('{}').", removed->name);
    }

    void StudioBridge::stop_all_views()
    {
        std::vector<std::unique_ptr<ViewStream>> removed;
        {
            auto lock = std::lock_guard(_views_mutex);
            if (_views.empty())
                return;

            removed = std::move(_views);
            _views.clear();
            for (auto& view : removed)
                _pending_shared_destroys.push_back(view->texture);
        }

        for (auto& view : removed)
            destroy_view_camera(view->camera_id);

        refresh_present_callback();
        TBX_TRACE_INFO("StudioBridge: all views stopped.");
    }

    void StudioBridge::refresh_present_callback()
    {
        auto rendering = _rendering.lock();
        if (!rendering)
            return;

        auto has_views = false;
        {
            auto lock = std::lock_guard(_views_mutex);
            has_views = !_views.empty();
        }

        // One callback fans every present out to whichever view owns that render target. Clearing it
        // when the last view goes away keeps the render lane free of editor work when nothing is
        // streaming.
        if (has_views)
        {
            rendering->set_pre_present_callback(
                [this](
                    tbx::IGraphicsBackend& backend,
                    const tbx::RenderTarget& output_target,
                    const tbx::Size& backbuffer_size)
                {
                    ensure_view_surfaces(backend, output_target, backbuffer_size);
                });
        }
        else
        {
            rendering->set_pre_present_callback({});
        }
    }

    void StudioBridge::ensure_view_surfaces(
        tbx::IGraphicsBackend& backend,
        const tbx::RenderTarget& output_target,
        const tbx::Size& backbuffer_size)
    {
        auto lock = std::lock_guard(_views_mutex);

        // Free the surfaces of views that have stopped. Any view's present drains the queue, so a
        // stopped view's GPU texture is reclaimed as soon as another view renders (or at shutdown).
        for (const auto& texture : _pending_shared_destroys)
            backend.destroy_shared_target(texture);
        _pending_shared_destroys.clear();

        for (auto& view : _views)
        {
            if (output_target.id != view->texture.id)
                continue;

            // Create the shared surface on first render (here on the render lane), then tell the
            // editor its cross-process handle exactly once. begin_frame draws the view straight
            // into this texture from the next frame on.
            if (!view->shared_attempted)
            {
                view->shared_attempted = true;
                auto info = tbx::SharedTargetInfo();
                if (const auto result = backend.create_shared_target(view->texture, backbuffer_size, info))
                {
                    view->shared = info;
                    view->shared_ready = true;
                }
                else
                {
                    TBX_TRACE_ERROR(
                        "StudioBridge: GPU texture sharing unavailable for view '{}': {}",
                        view->name,
                        result.get_report());
                }

                // Announce either way; a zero handle tells the editor to show its empty ghost.
                auto params = tbx::Json::object();
                params["name"] = view->name;
                params["sharedHandle"] = view->shared.shared_handle;
                params["width"] = view->shared.width;
                params["height"] = view->shared.height;
                params["format"] = "bgra8";
                _server.send_line(make_notification("view.surface", params));
            }
            else if (view->shared_ready && !view->presented_announced)
            {
                // The surface was created on an earlier present, so render_views has since drawn the
                // first real frame into it. Announce that exactly once so the editor knows loading is
                // truly done (a failed share never sets shared_ready, so it never "presents").
                view->presented_announced = true;
                auto params = tbx::Json::object();
                params["name"] = view->name;
                _server.send_line(make_notification("view.presented", params));
            }
            return;
        }
    }

    tbx::Entity StudioBridge::find_first_game_camera(tbx::World& world) const
    {
        // The world holds only the user's scene now (view cameras live in _view_registry), so the
        // game camera is simply its first camera.
        return world.first_with<tbx::Camera>();
    }

    tbx::Uuid StudioBridge::create_view_camera(
        tbx::World& world,
        const tbx::RenderTexture& texture,
        bool is_game)
    {
        // Both editor and game cameras start aligned with the scene's own (game) camera, so a new
        // view always opens somewhere useful. Editor cameras then move freely; game cameras get
        // re-synced to the game camera every frame in sync_game_views().
        auto initial_transform = tbx::Transform();
        auto game_camera = find_first_game_camera(world);
        if (game_camera.get_id().is_valid() && game_camera.has_component<tbx::Transform>())
            initial_transform =
                game_camera.get_component<tbx::Transform>().to_world_space(game_camera);
        initial_transform.id = tbx::Uuid::generate();

        // The editor camera keeps the game camera's position, but its direction is set later (in
        // update_editor_cameras) to face the scene once geometry has streamed in — facing the game
        // camera's authored direction would often open the view looking at empty sky on games that
        // orient their camera on play.

        // View cameras are tooling owned by the studio bridge, so they are created in the plugin's own
        // registry — never in the game world — and so never appear in the scene or the play snapshot.
        auto camera_entity = tbx::Entity(is_game ? "GameViewCamera" : "EditorCamera", _view_registry);
        camera_entity.add_component<tbx::Transform>(initial_transform);

        auto camera = tbx::Camera();
        if (is_game && game_camera.get_id().is_valid() && game_camera.has_component<tbx::Camera>())
            camera = game_camera.get_component<tbx::Camera>();
        camera.set_target(texture);
        camera_entity.add_component<tbx::Camera>(camera);
        return camera_entity.get_id();
    }

    void StudioBridge::destroy_view_camera(const tbx::Uuid& camera_id)
    {
        if (!camera_id.is_valid())
            return;

        auto camera_entity = _view_registry.get(camera_id);
        if (camera_entity.get_id().is_valid())
            _view_registry.remove(camera_entity);
    }

    void StudioBridge::render_views(const tbx::DeltaTime& dt)
    {
        auto rendering = _rendering.lock();
        if (!rendering || _graphics_settings == nullptr)
            return;

        // Our view cameras are not in the game world, so the engine's render loop never draws them.
        // Render each one ourselves into its own texture; Rendering::render draws the active game world
        // from whatever camera view it is given, regardless of which registry the camera lives in.
        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            if (!view->camera_id.is_valid())
                continue;

            auto camera_entity = _view_registry.get(view->camera_id);
            if (!camera_entity.get_id().is_valid())
                continue;

            const auto camera_view = tbx::CameraView::from_entity(camera_entity);
            if (!camera_view.is_valid)
                continue;

            rendering->render(
                dt,
                *_graphics_settings,
                camera_view,
                camera_view.camera.get_render_target());
        }
    }

    void StudioBridge::set_play_state(bool playing)
    {
        if (_is_playing == playing)
            return;

        // Entering play snapshots the world before it mutates, then unpauses the engine to start
        // simulating. Exiting pauses first, then restores the snapshot, so a play session leaves no
        // trace. The engine itself only knows about the neutral pause; play-mode is ours.
        if (playing)
        {
            snapshot_world();
            post_message<tbx::SetApplicationPausedRequest>(false);
        }
        else
        {
            post_message<tbx::SetApplicationPausedRequest>(true);
            restore_world();
        }

        _is_playing = playing;
    }

    void StudioBridge::report_mouse_lock()
    {
        if (!_server.has_client())
            return;

        auto input_manager = _input_manager.lock();
        // Only a playing game drives the lock mode; outside play the editor cursor is always free.
        const auto mode = (_is_playing && input_manager)
            ? input_manager->get_mouse_lock_mode()
            : tbx::MouseLockMode::UNLOCKED;
        if (mode == _last_reported_lock)
            return;

        _last_reported_lock = mode;
        auto params = tbx::Json::object();
        params["mode"] = to_lock_mode_name(mode);
        _server.send_line(make_notification("input.mouseLock", params));
    }

    void StudioBridge::snapshot_world()
    {
        _runtime_snapshot.clear();
        _global_snapshot.clear();

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return;

        for (const auto& entity : world->get_all())
        {
            // Preserve each entity's persistence so restore re-adds it as runtime or global as it was.
            if (world->is_global(entity.get_id()))
                _global_snapshot.absorb(entity);
            else
                _runtime_snapshot.absorb(entity);
        }
    }

    void StudioBridge::restore_world()
    {
        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (world)
        {
            // Remove every entity play spawned or mutated, then replay the snapshot with each entity's
            // original persistence. View cameras are unaffected — they live in _view_registry.
            auto to_destroy = std::vector<tbx::Uuid>();
            for (const auto& entity : world->get_all())
                to_destroy.push_back(entity.get_id());

            for (const auto& id : to_destroy)
            {
                auto entity = world->get(id);
                if (entity.get_id().is_valid())
                    world->destroy(entity);
            }

            world->add_entities(_runtime_snapshot);
            if (!_global_snapshot.is_empty())
            {
                auto globals = tbx::WorldGlobals();
                globals.entities = _global_snapshot;
                world->load_globals(globals);
            }
        }

        _runtime_snapshot.clear();
        _global_snapshot.clear();
    }

    void StudioBridge::sync_game_views()
    {
        auto lock = std::lock_guard(_views_mutex);
        const auto has_game_view =
            std::any_of(_views.begin(), _views.end(), [](const std::unique_ptr<ViewStream>& view)
                        { return view->is_game; });
        if (!has_game_view)
            return;

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return;

        auto game_camera = find_first_game_camera(*world);
        if (!game_camera.get_id().is_valid())
            return;

        const auto has_transform = game_camera.has_component<tbx::Transform>();
        const auto has_camera = game_camera.has_component<tbx::Camera>();
        for (auto& view : _views)
        {
            if (!view->is_game || !view->camera_id.is_valid())
                continue;

            auto mirror = _view_registry.get(view->camera_id);
            if (!mirror.get_id().is_valid())
                continue;

            // Mirror the game camera's pose and lens so the view shows exactly what the player sees,
            // but keep our own render target and a stable transform id.
            if (has_transform && mirror.has_component<tbx::Transform>())
            {
                auto& mirror_transform = mirror.get_component<tbx::Transform>();
                const auto transform_id = mirror_transform.id;
                mirror_transform = game_camera.get_component<tbx::Transform>().to_world_space(game_camera);
                mirror_transform.id = transform_id;
            }

            if (has_camera && mirror.has_component<tbx::Camera>())
            {
                auto& mirror_camera = mirror.get_component<tbx::Camera>();
                mirror_camera = game_camera.get_component<tbx::Camera>();
                mirror_camera.set_target(view->texture);
            }
        }
    }

    void StudioBridge::handle_view_input(const tbx::Json& params)
    {
        const auto name = params.value("view", std::string());
        if (name.empty())
            return;

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            if (view->name != name)
                continue;

            view->focused = params.value("focused", false);
            view->buttons = params.value("buttons", 0U);
            view->move_keys = params.value("moveKeys", 0U);
            // Mouse and wheel are deltas since the last message; accumulate until a frame consumes them.
            view->accumulated_mouse_dx += params.value("dx", 0.0F);
            view->accumulated_mouse_dy += params.value("dy", 0.0F);
            view->accumulated_wheel += params.value("wheel", 0.0F);

            // Game views also carry raw input for the engine input system: pressed SDL scancodes and the
            // absolute mouse position within the view.
            view->keys.clear();
            if (const auto keys = params.find("keys"); keys != params.end() && keys->is_array())
            {
                for (const auto& key : *keys)
                {
                    if (key.is_number_integer())
                        view->keys.push_back(key.get<int>());
                }
            }
            view->mouse_x = params.value("mouseX", 0.0F);
            view->mouse_y = params.value("mouseY", 0.0F);
            return;
        }
    }

    // Feeds the game's input system from the focused game view while playing, so a game running under
    // the editor reacts to input even though the engine window is hidden. Disabled otherwise, so the
    // game takes no input when not playing or when the game view is not focused.
    void StudioBridge::update_game_input()
    {
        auto input_manager = _input_manager.lock();
        if (!input_manager)
            return;

        auto keyboard = tbx::KeyboardState();
        auto mouse = tbx::MouseState();
        auto inject = false;

        {
            auto lock = std::lock_guard(_views_mutex);
            for (auto& view : _views)
            {
                if (!view->is_game)
                    continue;

                // The first focused game view drives the game while playing; build its input state
                // before consuming this view's accumulated deltas.
                if (_is_playing && view->focused && !inject)
                {
                    for (const auto key : view->keys)
                        keyboard.pressed_keys.insert(key);

                    // Studio button bits (0 left, 1 right, 2 middle) → SDL ids (1 left, 2 middle, 3 right).
                    if ((view->buttons & 0x1U) != 0U)
                        mouse.pressed_buttons.insert(1);
                    if ((view->buttons & 0x4U) != 0U)
                        mouse.pressed_buttons.insert(2);
                    if ((view->buttons & 0x2U) != 0U)
                        mouse.pressed_buttons.insert(3);

                    mouse.position = tbx::Vec2(view->mouse_x, view->mouse_y);
                    mouse.delta = tbx::Vec2(view->accumulated_mouse_dx, view->accumulated_mouse_dy);
                    mouse.wheel_delta = view->accumulated_wheel;
                    inject = true;
                }

                // Always consume deltas so they never burst when play/focus resumes.
                view->accumulated_mouse_dx = 0.0F;
                view->accumulated_mouse_dy = 0.0F;
                view->accumulated_wheel = 0.0F;
            }
        }

        input_manager->set_input_injection_enabled(inject);
        if (inject)
        {
            input_manager->set_injected_keyboard(keyboard);
            input_manager->set_injected_mouse(mouse);
        }
    }

    // Unity-style fly controls for the editor viewports: hold right mouse to look while WASD/QE move,
    // the wheel dollies forward, and middle-mouse drags pan. Applied frame-rate-independently to the
    // focused editor view's camera.
    void StudioBridge::update_editor_cameras(const tbx::DeltaTime& dt)
    {
        constexpr float LOOK_SENSITIVITY = 0.0045F; // radians per pixel
        constexpr float MOVE_SPEED = 6.0F;          // metres per second
        constexpr float WHEEL_DOLLY = 0.6F;         // metres per wheel notch
        constexpr float PAN_SENSITIVITY = 0.01F;    // metres per pixel
        constexpr float MAX_PITCH_DOT = 0.99F;      // stop just short of straight up/down
        constexpr uint32 RIGHT_BUTTON = 0x2U;
        constexpr uint32 MIDDLE_BUTTON = 0x4U;
        constexpr auto world_up = glm::vec3(0.0F, 1.0F, 0.0F);

        const auto seconds = static_cast<float>(dt.seconds);

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;

        auto lock = std::lock_guard(_views_mutex);
        for (auto& view : _views)
        {
            // Game views feed the game input system, not a fly camera (handled in update_game_input,
            // which owns consuming their deltas), so leave them untouched here.
            if (view->is_game)
                continue;

            if (!view->camera_id.is_valid() || !world)
            {
                view->accumulated_mouse_dx = 0.0F;
                view->accumulated_mouse_dy = 0.0F;
                view->accumulated_wheel = 0.0F;
                continue;
            }

            auto entity = _view_registry.get(view->camera_id);
            if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                continue;

            auto& transform = entity.get_component<tbx::Transform>();

            // One-time, once the scene's geometry has streamed in: aim the camera across it so the
            // viewport opens on the scene rather than wherever the game camera happened to be authored
            // facing. The aim is flattened to the horizon: a camera spawned near the scene's horizontal
            // centre has the centroid almost straight overhead, so looking *at* it would crane the view
            // up at the sky and make WASD fly straight up — opening level keeps navigation intuitive.
            if (view->needs_orient)
            {
                auto focus = glm::vec3(0.0F);
                if (compute_scene_focus(*world, focus))
                {
                    auto heading = focus - transform.position;
                    heading.y = 0.0F;
                    if (glm::length(heading) < 0.001F)
                    {
                        // Centroid is directly above/below: keep the current heading, also levelled.
                        heading = transform.rotation * glm::vec3(0.0F, 0.0F, -1.0F);
                        heading.y = 0.0F;
                    }
                    if (glm::length(heading) < 0.001F)
                        heading = glm::vec3(0.0F, 0.0F, -1.0F);

                    transform.rotation = look_rotation(heading, world_up);
                    view->needs_orient = false;
                }
            }

            // Only the focused editor view drives the fly camera; clear pending deltas otherwise so
            // they do not burst when focus returns.
            if (!view->focused)
            {
                view->accumulated_mouse_dx = 0.0F;
                view->accumulated_mouse_dy = 0.0F;
                view->accumulated_wheel = 0.0F;
                continue;
            }

            // Look only while right mouse is held, and rotate the camera's *current* orientation
            // incrementally (world-up yaw + local-right pitch) so it stays roll-free and the spawn
            // heading — which faces the scene — is preserved until the user actually looks around.
            if ((view->buttons & RIGHT_BUTTON) != 0U
                && (view->accumulated_mouse_dx != 0.0F || view->accumulated_mouse_dy != 0.0F))
            {
                auto rotated = glm::angleAxis(
                                   -view->accumulated_mouse_dx * LOOK_SENSITIVITY, world_up)
                               * transform.rotation;
                const auto local_right = glm::normalize(rotated * glm::vec3(1.0F, 0.0F, 0.0F));
                const auto pitched =
                    glm::angleAxis(-view->accumulated_mouse_dy * LOOK_SENSITIVITY, local_right)
                    * rotated;
                if (std::abs((pitched * glm::vec3(0.0F, 0.0F, -1.0F)).y) < MAX_PITCH_DOT)
                    rotated = pitched;
                transform.rotation = glm::normalize(rotated);
            }

            const auto rotation = transform.rotation;
            const auto forward = rotation * glm::vec3(0.0F, 0.0F, -1.0F);
            const auto right = rotation * glm::vec3(1.0F, 0.0F, 0.0F);

            auto move = glm::vec3(0.0F);
            if ((view->move_keys & 0x01U) != 0U)
                move += forward;
            if ((view->move_keys & 0x02U) != 0U)
                move -= forward;
            if ((view->move_keys & 0x04U) != 0U)
                move -= right;
            if ((view->move_keys & 0x08U) != 0U)
                move += right;
            if ((view->move_keys & 0x10U) != 0U)
                move += world_up;
            if ((view->move_keys & 0x20U) != 0U)
                move -= world_up;
            if (glm::dot(move, move) > 0.0F)
                transform.position += glm::normalize(move) * (MOVE_SPEED * seconds);

            if (view->accumulated_wheel != 0.0F)
                transform.position += forward * (view->accumulated_wheel * WHEEL_DOLLY);

            if ((view->buttons & MIDDLE_BUTTON) != 0U)
            {
                transform.position += right * (-view->accumulated_mouse_dx * PAN_SENSITIVITY);
                transform.position += world_up * (view->accumulated_mouse_dy * PAN_SENSITIVITY);
            }

            view->accumulated_mouse_dx = 0.0F;
            view->accumulated_mouse_dy = 0.0F;
            view->accumulated_wheel = 0.0F;
        }
    }

    tbx::Json StudioBridge::handle_describe_world() const
    {
        auto result = tbx::Json::object();
        auto entities = tbx::Json::array();

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock()
                                   : std::shared_ptr<tbx::World>();
        if (world)
        {
            for (const auto& entity : world->get_all())
            {
                // The editor needs every field plus reflection metadata, so serialize with both
                // defaults and attribute enrichment on.
                auto entity_json = tbx::Json::parse(
                    tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true),
                    nullptr,
                    false);
                if (!entity_json.is_discarded() && entity_json.is_object())
                {
                    // Globalness is World-level state (not on the entity/registry), so the describe layer
                    // tags it for the editor's Globals section.
                    entity_json["is_global"] = world->is_global(entity.get_id());
                    entities.push_back(std::move(entity_json));
                }
            }
        }

        result["entities"] = std::move(entities);
        result["component_types"] = component_type_icons();
        return result;
    }

    tbx::Json StudioBridge::component_type_icons() const
    {
        // A side table of component-type icons ([[tbx::icon]]), keyed by wire name, so the inspector can
        // badge component headers without bloating every persisted component payload. The icon is carried
        // on each component's serializable registration.
        auto component_types = tbx::Json::object();
        for (const auto& registration : tbx::get_entity_component_type_registrations())
        {
            if (registration.name.empty() || registration.icon.empty())
                continue;

            auto icon = tbx::Json::object();
            icon["icon"] = registration.icon;
            if (!registration.icon_color.empty())
                icon["iconColor"] = registration.icon_color;
            component_types[registration.name] = std::move(icon);
        }
        return component_types;
    }

    Result StudioBridge::describe_entity(const tbx::Json& params, tbx::Json& out_reply) const
    {
        // Reuses the reflect entity resolver (reads/validates entityId against the active world).
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        // Same per-entity shape world.describe emits: every field plus reflection metadata. Lets the
        // editor re-query just the selected entity to stay in sync with the running game.
        auto entity_json = tbx::Json::parse(
            tbx::Entity::serialize(entity, /*include_defaults=*/true, /*include_attributes=*/true),
            nullptr,
            false);
        if (entity_json.is_discarded() || !entity_json.is_object())
            return Result(false, "Failed to serialize entity.");

        if (auto world_manager = _world_manager.lock())
            if (auto world = world_manager->get_active_world().lock())
                entity_json["is_global"] = world->is_global(entity.get_id());

        out_reply["entity"] = std::move(entity_json);
        out_reply["component_types"] = component_type_icons();
        return Result::OK;
    }

    Result StudioBridge::set_entity_global(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        const auto id = tbx::Uuid(id_iterator->get<uint32>());
        if (!world->has(id))
            return Result(false, "Entity not found.");

        world->set_global(id, params.value("global", false));
        return Result::OK;
    }

    Result StudioBridge::save_world() const
    {
        auto world_manager = _world_manager.lock();
        if (!world_manager)
            return Result(false, "No active world.");

        if (!world_manager->save_active_world())
            return Result(false, "Failed to save the active world.");

        return Result::OK;
    }

    Result StudioBridge::save_asset(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto type = params.value("type", std::string());
        const auto path = params.value("path", std::string());
        if (type.empty() || path.empty())
            return Result(false, "Missing 'type' or 'path'.");

        const auto value_iterator = params.find("json");
        if (value_iterator == params.end() || !value_iterator->is_object())
            return Result(false, "Missing 'json' body.");

        const auto registration = tbx::get_asset_type_registration(type);
        if (!registration || !registration->create_asset || !registration->read_body)
            return Result(false, "Unknown or non-serializable asset type: " + type);

        auto asset = registration->create_asset();
        if (!asset)
            return Result(false, "Could not create asset of type: " + type);

        if (auto read = registration->read_body(value_iterator->dump(), asset.get()); !read)
            return read;

        auto asset_manager = _asset_manager.lock();
        auto serialization =
            asset_manager ? asset_manager->get_serialization_registry().lock() : nullptr;
        if (!serialization)
            return Result(false, "No serialization registry.");

        return serialization->write(path, *registration, asset.get());
    }

    Result StudioBridge::set_entity_name(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        entity.set_name(params.value("name", std::string()));
        return Result::OK;
    }

    // Lower-cased file extension without the leading dot, used as the asset's editor "type" so the
    // handle picker can filter (e.g. "mat", "png", "world"). Empty extensions fall back to "asset".
    static std::string asset_type_from_path(const std::filesystem::path& path)
    {
        auto extension = path.extension().string();
        if (!extension.empty() && extension.front() == '.')
            extension.erase(extension.begin());
        if (extension.empty())
            return "asset";

        for (auto& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        return extension;
    }

    tbx::Json StudioBridge::handle_list_assets() const
    {
        auto result = tbx::Json::object();
        auto assets = tbx::Json::array();
        auto scripts = tbx::Json::array();

        if (auto asset_manager = _asset_manager.lock())
        {
            for (const auto& entry : asset_manager->get_registered_assets())
            {
                const auto display_name = entry.resolved_path.empty()
                                              ? entry.normalized_path
                                              : entry.resolved_path.stem().string();

                auto asset = tbx::Json::object();
                asset["id"] = entry.asset_id.value;
                asset["name"] = display_name;
                asset["type"] = asset_type_from_path(entry.resolved_path);
                asset["path"] = entry.normalized_path;
                assets.push_back(std::move(asset));
            }
        }

        // The script catalog is the set of registered asset types that carry runtime script glue
        // (regular assets leave bind_runtime empty). It lets the editor label/validate script refs.
        for (const auto& registration : tbx::get_asset_type_registrations())
        {
            if (!registration.bind_runtime)
                continue;

            auto script = tbx::Json::object();
            script["name"] = registration.type_name;
            script["version"] = registration.version;
            scripts.push_back(std::move(script));
        }

        result["assets"] = std::move(assets);
        result["scripts"] = std::move(scripts);
        return result;
    }

    tbx::Json StudioBridge::describe_settings() const
    {
        // Hand the editor the full AppSettings schema with every field's engine default — graphics, physics,
        // async, etc. The project's own AppSettings.json is lean (only the values it overrides), so the editor
        // merges its values over these defaults and diffs against them again to save leanly. The enriched
        // per-field shape (type tokens, enum choices, and the plugins vector's element_template that make that
        // list editable) is produced by the engine-side describe helper, which enters the attribute scope in
        // the engine module where the generated serialize runs — serializing across the plugin boundary here
        // would silently fall back to the lean { type, value } form.
        const auto schema = tbx::describe_serializable_asset("AppSettings");
        auto reply = tbx::Json::object();
        reply["settings"] = schema.empty() ? tbx::Json::object() : tbx::Json::parse(schema);
        return reply;
    }

    Result StudioBridge::describe_asset(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("assetId");
        if (id_iterator == params.end() || !id_iterator->is_number())
            return Result(false, "Missing or invalid 'assetId'.");
        const auto asset_id = id_iterator->get<uint32>();

        auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "No asset manager.");

        // Find the registered asset by id and confirm it is a material before loading. The id matches the
        // value the handle picker wrote (editor.listAssets advertises entry.asset_id.value as the id).
        for (const auto& entry : asset_manager->get_registered_assets())
        {
            if (entry.asset_id.value != asset_id)
                continue;

            if (asset_type_from_path(entry.resolved_path) != "mat")
                return Result(false, "Asset is not a material.");

            const auto material =
                asset_manager->load<tbx::Material>(tbx::Handle(entry.normalized_path, entry.asset_id));
            if (!material)
                return Result(false, "Failed to load material.");

            // Same enriched shape entity.describe emits per field (every field plus reflection metadata),
            // so the editor parses the base parameters/textures with the existing JsonParser path.
            const auto include_all = tbx::OmitDefaultFieldsScope(false);
            const auto include_attrs = tbx::AttributeSerializationScope(true);
            auto material_json = tbx::Json::object();
            serialize(material_json, *material);
            out_reply["material"] = std::move(material_json);
            return Result::OK;
        }

        return Result(false, "Asset not found.");
    }

    Result StudioBridge::apply_component(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto value_iterator = params.find("value");
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        const auto entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return tbx::apply_component(entity, component, value_iterator->dump());
    }

    // Reads an optional "parent" param: a missing/0 value means the root (an invalid Uuid).
    static tbx::Uuid read_parent_param(const tbx::Json& params)
    {
        const auto parent_iterator = params.find("parent");
        if (parent_iterator == params.end() || !parent_iterator->is_number_unsigned())
            return tbx::Uuid();

        const auto parent_value = parent_iterator->get<uint32>();
        return parent_value == 0U ? tbx::Uuid() : tbx::Uuid(parent_value);
    }

    Result StudioBridge::create_entity(const tbx::Json& params, tbx::Json& out_reply) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        const auto name = params.value("name", std::string());
        const auto parent = read_parent_param(params);
        if (parent.is_valid() && !world->has(parent))
            return Result(false, "Parent entity not found.");

        auto entity =
            parent.is_valid() ? world->create_entity(name, parent) : world->create_entity(name);
        if (!entity.get_id().is_valid())
            return Result(false, "Failed to create entity.");

        // Append after the last existing sibling so a new entity lands at the bottom of its list.
        auto max_order = -1;
        for (const auto& sibling : world->get_all())
        {
            if (sibling.get_id().value != entity.get_id().value
                && sibling.get_parent().value == parent.value)
                max_order = std::max(max_order, sibling.get_order());
        }
        entity.set_order(max_order + 1);

        out_reply["id"] = entity.get_id().value;
        return Result::OK;
    }

    Result StudioBridge::destroy_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        const auto root_id = tbx::Uuid(id_iterator->get<uint32>());
        if (!world->has(root_id))
            return Result(false, "Entity not found.");

        // Collect the entity and every descendant before destroying any, then destroy deepest-first so a
        // child is never left pointing at a freed parent.
        auto doomed = std::vector<tbx::Uuid> {root_id};
        for (size index = 0U; index < doomed.size(); ++index)
        {
            const auto parent_id = doomed[index];
            for (const auto& candidate : world->get_all())
            {
                if (candidate.get_parent().value == parent_id.value)
                    doomed.push_back(candidate.get_id());
            }
        }

        for (auto iterator = doomed.rbegin(); iterator != doomed.rend(); ++iterator)
        {
            auto entity = world->get(*iterator);
            if (entity.get_id().is_valid())
                world->destroy(entity);
        }

        return Result::OK;
    }

    Result StudioBridge::move_entity(const tbx::Json& params) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        const auto index_iterator = params.find("index");
        if (index_iterator == params.end() || !index_iterator->is_number_integer())
            return Result(false, "Missing or invalid 'index'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        const auto entity_id = tbx::Uuid(id_iterator->get<uint32>());
        auto entity = world->get(entity_id);
        if (!entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        const auto parent = read_parent_param(params);
        if (parent.is_valid() && !world->has(parent))
            return Result(false, "Target parent not found.");

        // Reject moving an entity beneath itself or its own descendant (which would detach the subtree
        // into a cycle). Walk up from the target parent looking for the entity being moved.
        for (auto ancestor = parent; ancestor.is_valid();
             ancestor = world->get(ancestor).get_parent())
        {
            if (ancestor.value == entity_id.value)
                return Result(false, "Cannot move an entity into its own descendant.");
        }

        entity.set_parent(parent);

        // Gather the destination siblings (excluding the moved entity) in their current order, splice the
        // moved entity in at the requested slot, then renumber 0..n so order stays dense and stable.
        auto siblings = std::vector<tbx::Entity>();
        for (const auto& candidate : world->get_all())
        {
            if (candidate.get_id().value != entity_id.value
                && candidate.get_parent().value == parent.value)
                siblings.push_back(candidate);
        }
        std::ranges::sort(
            siblings,
            [](const tbx::Entity& left, const tbx::Entity& right)
            {
                if (left.get_order() != right.get_order())
                    return left.get_order() < right.get_order();
                return left.get_id().value < right.get_id().value;
            });

        auto target = std::clamp(index_iterator->get<int>(), 0, static_cast<int>(siblings.size()));
        siblings.insert(siblings.begin() + target, entity);
        for (auto order = 0; order < static_cast<int>(siblings.size()); ++order)
            siblings[order].set_order(order);

        return Result::OK;
    }

    Result StudioBridge::resolve_reflect_entity(
        const tbx::Json& params,
        tbx::Entity& out_entity) const
    {
        if (!params.is_object())
            return Result(false, "Missing request parameters.");

        const auto id_iterator = params.find("entityId");
        if (id_iterator == params.end() || !id_iterator->is_number_unsigned())
            return Result(false, "Missing or invalid 'entityId'.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world.");

        out_entity = world->get(tbx::Uuid(id_iterator->get<uint32>()));
        if (!out_entity.get_id().is_valid())
            return Result(false, "Entity not found.");

        return Result::OK;
    }

    Result StudioBridge::reflect_get(const tbx::Json& params, tbx::Json& out_node) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        auto node_json = std::string();
        if (const auto result =
                tbx::serialize_component_property(entity, component, property, node_json);
            !result)
            return result;

        out_node = tbx::Json::parse(node_json, nullptr, false);
        if (out_node.is_discarded())
            return Result(false, "Engine produced an invalid property node.");

        return Result::OK;
    }

    Result StudioBridge::reflect_set(const tbx::Json& params) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        const auto value_iterator = params.find("value");
        if (value_iterator == params.end())
            return Result(false, "Missing 'value'.");

        return tbx::apply_component_property(
            entity,
            component,
            property,
            value_iterator->dump());
    }

    Result StudioBridge::reflect_reset(const tbx::Json& params) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        return tbx::reset_component_property(entity, component, property);
    }

    Result StudioBridge::reflect_is_default(
        const tbx::Json& params,
        bool& out_is_default) const
    {
        auto entity = tbx::Entity();
        if (const auto resolved = resolve_reflect_entity(params, entity); !resolved)
            return resolved;

        const auto component = params.value("component", std::string());
        if (component.empty())
            return Result(false, "Missing 'component'.");

        const auto property = params.value("property", std::string());
        if (property.empty())
            return Result(false, "Missing 'property'.");

        return tbx::is_component_property_default(
            entity,
            component,
            property,
            out_is_default);
    }

}
