#pragma once
#include "frame_capture.h"
#include "rpc_server.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/log_level.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>

namespace tbx::rpc_communication
{
    /// @brief
    /// Purpose: Bridges the engine to external tooling (Toybox Studio) over RPC, translating
    /// editor requests into engine messages and streaming engine state back out.
    /// @details
    /// Ownership: Owns the RPC server and its registered log listener. Thread Safety: Not
    /// thread-safe; requests are drained and handled on the main thread.
    [[tbx::plugin(
        name = "RpcCommunication",
        version = "0.1.0",
        category = tbx::PluginCategory::DEFAULT)]];
    class TBX_PLUGIN_API RpcCommunication final : public tbx::Plugin
    {
      public:
        RpcCommunication() = default;
        ~RpcCommunication() noexcept override = default;

      public:
        RpcCommunication(const RpcCommunication&) = delete;
        RpcCommunication& operator=(const RpcCommunication&) = delete;
        RpcCommunication(RpcCommunication&&) noexcept = delete;
        RpcCommunication& operator=(RpcCommunication&&) noexcept = delete;

      public:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void forward_log(tbx::LogLevel level, const std::string& message);
        void write_editor_log(const tbx::Json& params);
        void handle_request_line(const std::string& line);
        void handle_set_log_colors(const tbx::Json& params);
        tbx::Json handle_hello() const;
        tbx::Json handle_describe_world() const;
        tbx::Json handle_list_assets() const;
        Result apply_component(const tbx::Json& params) const;
        Result resolve_reflect_entity(const tbx::Json& params, tbx::Entity& out_entity) const;
        Result reflect_get(const tbx::Json& params, tbx::Json& out_node) const;
        Result reflect_set(const tbx::Json& params) const;
        Result reflect_reset(const tbx::Json& params) const;
        Result reflect_is_default(const tbx::Json& params, bool& out_is_default) const;
        tbx::Json reflect_describe_type(const tbx::Json& params) const;
        Result start_view();
        void stop_view();
        void create_editor_camera(tbx::World& world);
        void destroy_editor_camera();

      private:
        RpcServer _server = {};
        FrameCapture _frame_capture = {};
        std::weak_ptr<tbx::WorldManager> _world_manager = {};
        std::weak_ptr<tbx::Rendering> _rendering = {};
        std::weak_ptr<tbx::AssetManager> _asset_manager = {};
        tbx::RenderTexture _editor_view_texture = {};
        tbx::Uuid _editor_camera_id = {};
        std::string _app_name = {};
        std::string _view_name = {};
        uint16 _port = 0U;
        uint _log_listener_id = 0U;
        bool _had_client = false;
    };
}
