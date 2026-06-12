#include "rpc_communication_plugin.h"
#include "rpc_protocol.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/color.h"
#include "tbx/types/components/transform.h"
#include <cstdlib>
#include <format>
#include <string>

namespace tbx::rpc_communication
{
    constexpr uint16 DEFAULT_RPC_PORT = 17890U;
    constexpr std::string_view EDITOR_ENTITY_TAG = "tbx_editor";
    constexpr uint32 EDITOR_VIEW_WIDTH = 1280U;
    constexpr uint32 EDITOR_VIEW_HEIGHT = 720U;
    constexpr int PROTOCOL_VERSION = 1;

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

    void RpcCommunication::on_attach()
    {
        _port = read_port_from_env();
        const auto port = _port;
        auto result = _server.start(port);
        if (!result)
        {
            TBX_TRACE_ERROR(
                "RpcCommunication: failed to start the RPC server on port {}: {}",
                port,
                result.get_report());
            return;
        }

        TBX_TRACE_INFO("RpcCommunication: listening on 127.0.0.1:{}", port);
        _log_listener_id = tbx::Log::get_instance().add_listener(
            [this](tbx::LogLevel level, const std::string& message)
            { forward_log(level, message); });
    }

    void RpcCommunication::on_detach()
    {
        stop_view();
        if (_log_listener_id != 0U)
        {
            tbx::Log::get_instance().remove_listener(_log_listener_id);
            _log_listener_id = 0U;
        }

        _server.stop();
        _app_name.clear();
        _had_client = false;
    }

    void RpcCommunication::on_update(const tbx::DeltaTime& dt)
    {
        const auto has_client = _server.has_client();
        if (has_client != _had_client)
        {
            TBX_TRACE_INFO("RpcCommunication: editor {}.", has_client ? "connected" : "disconnected");
            if (!has_client)
                stop_view();

            _had_client = has_client;
        }

        for (const auto& line : _server.take_received_lines())
            handle_request_line(line);
    }

    void RpcCommunication::on_recieve_message(tbx::Message& msg)
    {
        if (auto initialized_event = tbx::handle_message<tbx::ApplicationInitializedEvent>(msg))
        {
            auto& application = initialized_event->get().application;
            _app_name = application.get_name();
            auto& services = application.get_service_provider();
            _world_manager = services.try_get_service<tbx::WorldManager>();
            _rendering = services.try_get_service<tbx::Rendering>();
        }
    }

    // Set on the thread that is writing an editor-originated log so the listener below does not echo
    // that line straight back to the editor (which already displayed it locally). Thread-local because
    // the log listener runs synchronously on whatever thread called into tbx::Log.
    static thread_local bool t_suppress_log_forward = false;

    void RpcCommunication::forward_log(tbx::LogLevel level, const std::string& message)
    {
        if (t_suppress_log_forward || !_server.has_client())
            return;

        auto params = tbx::Json::object();
        params["level"] = to_log_level_name(level);
        params["message"] = message;
        _server.send_line(make_notification("engine.log", params));
    }

    void RpcCommunication::write_editor_log(const tbx::Json& params)
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

        // Route editor lines through the engine's normal logging (file + console + listeners) but skip
        // the RPC echo so the editor doesn't show its own line twice.
        t_suppress_log_forward = true;
        tbx::Log::get_instance().write_internal(level, "Studio", 0, message);
        t_suppress_log_forward = false;
    }

    void RpcCommunication::handle_set_log_colors(const tbx::Json& params)
    {
        auto& log = tbx::Log::get_instance();
        if (params.contains("info"))
            log.set_color(tbx::LogLevel::INFO, parse_hex_color(params.value("info", std::string())));

        if (params.contains("warning"))
            log.set_color(
                tbx::LogLevel::WARNING, parse_hex_color(params.value("warning", std::string())));

        if (params.contains("error"))
        {
            const auto error_color = parse_hex_color(params.value("error", std::string()));
            log.set_color(tbx::LogLevel::ERROR, error_color);
            log.set_color(tbx::LogLevel::CRITICAL, error_color);
        }
    }

    void RpcCommunication::handle_request_line(const std::string& line)
    {
        const auto message = try_parse_message(line);
        if (!message)
        {
            _server.send_line(
                make_error_response(tbx::Json(), JSON_RPC_PARSE_ERROR_CODE, "Failed to parse request."));
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
        else if (method == "entity.setComponent")
        {
            const auto result = apply_component(request.value("params", tbx::Json::object()));
            if (result)
                _server.send_line(make_result_response(id, tbx::Json::object()));
            else
                _server.send_line(
                    make_error_response(id, JSON_RPC_APPLY_FAILED_CODE, result.get_report()));
        }
        else if (method == "view.start")
        {
            auto view_result = start_view();
            if (view_result)
            {
                auto view_info = tbx::Json::object();
                view_info["name"] = _view_name;
                view_info["format"] = "bgra8";
                _server.send_line(make_result_response(id, view_info));
            }
            else
            {
                _server.send_line(
                    make_error_response(id, JSON_RPC_VIEW_UNAVAILABLE_CODE, view_result.get_report()));
            }
        }
        else if (method == "view.stop")
        {
            stop_view();
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "engine.setPaused")
        {
            const auto params = request.value("params", tbx::Json::object());
            const auto is_paused = params.value("isPaused", false);
            post_message<tbx::SetApplicationPausedRequest>(is_paused);
            _server.send_line(make_result_response(id, tbx::Json::object()));
        }
        else if (method == "editor.log")
        {
            // Notification from the editor: write its line into the engine's unified log. No response.
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
            TBX_TRACE_INFO("RpcCommunication: shutdown requested by the editor.");
            post_message<tbx::ExitApplicationRequest>();
        }
        else if (!id.is_null())
        {
            _server.send_line(
                make_error_response(id, JSON_RPC_METHOD_NOT_FOUND_CODE, "Unknown method: " + method));
        }
    }

    tbx::Json RpcCommunication::handle_hello() const
    {
        auto result = tbx::Json::object();
        result["protocolVersion"] = PROTOCOL_VERSION;
        result["engine"] = "Toybox";
        result["app"] = _app_name;
        return result;
    }

    Result RpcCommunication::start_view()
    {
        auto rendering = _rendering.lock();
        if (!rendering)
            return Result(false, "Rendering service is unavailable.");

        auto world_manager = _world_manager.lock();
        auto world = world_manager ? world_manager->get_active_world().lock() : nullptr;
        if (!world)
            return Result(false, "No active world to view.");

        _view_name = std::format("ToyboxStudioFrame_{}", _port);
        auto result = _frame_capture.start(_view_name);
        if (!result)
            return result;

        create_editor_camera(*world);

        // Only the editor camera's frames feed the shared view; other cameras (the game view)
        // keep rendering to their own targets untouched.
        rendering->set_pre_present_callback(
            [this](
                tbx::IGraphicsBackend& backend,
                const tbx::RenderTarget& output_target,
                const tbx::Size& backbuffer_size)
            {
                if (output_target.id == _editor_view_texture.id)
                    _frame_capture.capture_backbuffer(backend, backbuffer_size);
            });
        TBX_TRACE_INFO("RpcCommunication: frame streaming started ('{}').", _view_name);
        return Result::OK;
    }

    void RpcCommunication::stop_view()
    {
        if (!_frame_capture.is_active())
            return;

        if (auto rendering = _rendering.lock())
            rendering->set_pre_present_callback({});

        destroy_editor_camera();
        _frame_capture.stop();
        TBX_TRACE_INFO("RpcCommunication: frame streaming stopped.");
    }

    void RpcCommunication::create_editor_camera(tbx::World& world)
    {
        _editor_view_texture = tbx::RenderTexture("StudioEditorView");
        _editor_view_texture.size = tbx::Size {EDITOR_VIEW_WIDTH, EDITOR_VIEW_HEIGHT};

        // Spawn aligned with the scene's own camera so the editor view starts somewhere useful.
        auto initial_transform = tbx::Transform();
        auto scene_camera = world.first_with<tbx::Camera>();
        if (scene_camera.get_id().is_valid() && scene_camera.has_component<tbx::Transform>())
            initial_transform =
                scene_camera.get_component<tbx::Transform>().to_world_space(scene_camera);
        initial_transform.id = tbx::Uuid::generate();

        auto editor_camera_entity = world.create_entity("EditorCamera");
        editor_camera_entity.set_tag(std::string(EDITOR_ENTITY_TAG));
        editor_camera_entity.add_component<tbx::Transform>(initial_transform);

        auto editor_camera = tbx::Camera();
        editor_camera.set_target(_editor_view_texture);
        editor_camera_entity.add_component<tbx::Camera>(editor_camera);
        _editor_camera_id = editor_camera_entity.get_id();
    }

    void RpcCommunication::destroy_editor_camera()
    {
        if (!_editor_camera_id.is_valid())
            return;

        if (auto world_manager = _world_manager.lock())
        {
            if (auto world = world_manager->get_active_world().lock())
            {
                auto editor_camera_entity = world->get(_editor_camera_id);
                if (editor_camera_entity.get_id().is_valid())
                    world->destroy(editor_camera_entity);
            }
        }

        _editor_camera_id = {};
    }

    tbx::Json RpcCommunication::handle_describe_world() const
    {
        auto result = tbx::Json::object();
        auto entities = tbx::Json::array();

        auto world_manager = _world_manager.lock();
        auto world =
            world_manager ? world_manager->get_active_world().lock() : std::shared_ptr<tbx::World>();
        if (world)
        {
            for (const auto& entity : world->get_all())
            {
                // Editor-owned entities are tooling internals, not part of the user's scene.
                if (entity.get_tag() == EDITOR_ENTITY_TAG)
                    continue;

                auto entity_json = tbx::Json::parse(tbx::Entity::serialize(entity), nullptr, false);
                if (!entity_json.is_discarded() && entity_json.is_object())
                    entities.push_back(std::move(entity_json));
            }
        }

        result["entities"] = std::move(entities);
        return result;
    }

    Result RpcCommunication::apply_component(const tbx::Json& params) const
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

        return tbx::Entity::apply_component_json(entity, component, value_iterator->dump());
    }
}
