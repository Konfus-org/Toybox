#pragma once
#include "rpc_server.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/log_level.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <memory>
#include <mutex>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: One editor-facing render view — a camera drawing into a dedicated render texture
    /// that is a shared GPU surface the editor samples directly (zero copy, no readback).
    /// @details
    /// Ownership: Owns the editor camera entity it spawned. The shared GPU texture itself is owned
    /// by the graphics backend (keyed by texture id) and torn down on the render lane. Game views
    /// mirror the game camera every frame.
    struct ViewStream
    {
        std::string name = {};
        tbx::RenderTexture texture = {};
        tbx::Uuid camera_id = {};
        bool is_game = false;

        // The view's shared GPU surface. Created lazily on the render lane the first time the view
        // renders, then announced to the editor (view.surface). shared.shared_handle == 0 once
        // attempted means sharing was unavailable — the editor shows its empty ghost instead.
        tbx::SharedTargetInfo shared = {};
        bool shared_attempted = false;
        bool shared_ready = false;
        // Set once a real frame has actually been drawn into the shared surface (the present after
        // it was created), then announced to the editor (view.presented) so it can drop its loading
        // ghost only when there is genuinely something to show.
        bool presented_announced = false;

        // Input forwarded from the editor's matching viewport (drives the editor fly camera). Buttons
        // and move keys are the latest held state; the mouse/wheel deltas accumulate between engine
        // frames and are consumed when the fly camera applies them.
        bool focused = false;
        uint32 buttons = 0U;   // bit0 = left, bit1 = right, bit2 = middle
        uint32 move_keys = 0U; // bit0 fwd, 1 back, 2 left, 3 right, 4 up, 5 down
        float accumulated_mouse_dx = 0.0F;
        float accumulated_mouse_dy = 0.0F;
        float accumulated_wheel = 0.0F;
        // Editor cameras aim at the scene once, after its geometry has streamed in (the world loads
        // a few frames after the view starts).
        bool needs_orient = true;

        // Raw game input forwarded for a game view (fed into the engine input system while playing and
        // this view is focused): pressed SDL scancodes plus the absolute mouse position in the view.
        std::vector<int> keys = {};
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;
    };

    /// @brief
    /// Purpose: Bridges the engine to external tooling (Toybox Studio) over RPC, translating
    /// editor requests into engine messages and streaming engine state back out.
    /// @details
    /// Ownership: Owns the RPC server and its registered log listener. Thread Safety: Not
    /// thread-safe; requests are drained and handled on the main thread.
    [[tbx::plugin(
        name = "StudioBridge",
        version = "0.1.0",
        category = tbx::PluginCategory::DEFAULT)]];
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

      private:
        void forward_log(tbx::LogLevel level, const std::string& message);
        void write_editor_log(const tbx::Json& params);
        void handle_request_line(const std::string& line);
        void handle_set_log_colors(const tbx::Json& params);
        tbx::Json handle_hello() const;
        tbx::Json handle_describe_world() const;
        // The component-type icon side table shared by world.describe and entity.describe.
        tbx::Json component_type_icons() const;
        // Serializes one entity (the same shape world.describe emits per entity) so the editor can keep the
        // selected entity in sync with the running game; replies { entity, component_types }.
        Result describe_entity(const tbx::Json& params, tbx::Json& out_reply) const;
        // Loads a material asset by id and serializes it with reflection metadata so the editor can show
        // a MaterialInstance's base values (the slots its overrides apply on top of); replies { material }.
        Result describe_asset(const tbx::Json& params, tbx::Json& out_reply) const;
        // Serializes a default-constructed AppSettings with reflection metadata (the full settings schema +
        // engine defaults + the plugins vector's element_template) so the editor can render every setting and
        // make the plugins list editable; replies { settings }.
        tbx::Json describe_settings() const;
        tbx::Json handle_list_assets() const;
        Result apply_component(const tbx::Json& params) const;
        // World-as-list editing: create a child/root entity (replies { id }), destroy an entity and its
        // subtree, or move an entity to a new parent and sibling index (reorder + reparent in one call).
        Result create_entity(const tbx::Json& params, tbx::Json& out_reply) const;
        Result destroy_entity(const tbx::Json& params) const;
        Result move_entity(const tbx::Json& params) const;
        // Renames an entity in place (the editor's inline rename in the world list).
        Result set_entity_name(const tbx::Json& params) const;
        // Promotes/demotes an entity between global (full-lifetime resident) and ordinary scene entity.
        Result set_entity_global(const tbx::Json& params) const;
        Result resolve_reflect_entity(const tbx::Json& params, tbx::Entity& out_entity) const;
        Result reflect_get(const tbx::Json& params, tbx::Json& out_node) const;
        Result reflect_set(const tbx::Json& params) const;
        Result reflect_reset(const tbx::Json& params) const;
        Result reflect_is_default(const tbx::Json& params, bool& out_is_default) const;
        Result start_view(bool is_game, std::string& out_name);
        void stop_view(const std::string& name);
        void stop_all_views();
        tbx::Uuid create_view_camera(
            tbx::World& world,
            const tbx::RenderTexture& texture,
            bool is_game);
        void destroy_view_camera(const tbx::Uuid& camera_id);
        tbx::Entity find_first_game_camera(tbx::World& world) const;
        void sync_game_views();
        void handle_view_input(const tbx::Json& params);
        void update_editor_cameras(const tbx::DeltaTime& dt);
        void update_game_input();
        // Renders each view's camera (owned by _view_registry, outside the game world) into its render
        // texture; the engine's own loop only draws the game world's cameras.
        void render_views(const tbx::DeltaTime& dt);
        void refresh_present_callback();
        // Runs on the render lane (the only place GL/D3D shared-target work is safe): frees the
        // shared surfaces of stopped views, then creates the rendering view's surface on first use
        // and announces its cross-process handle to the editor (view.surface).
        void ensure_view_surfaces(
            tbx::IGraphicsBackend& backend,
            const tbx::RenderTarget& output_target,
            const tbx::Size& backbuffer_size);

        // Play-mode is owned here, not the engine: snapshot the whole game world on entering play and
        // restore it on exit, while driving the engine's neutral pause to gate simulation.
        void set_play_state(bool playing);
        void snapshot_world();
        void restore_world();

        // The engine runs hidden, so when the game asks for relative-mouse (mouselook) the editor's game
        // panel must mirror it. Each frame, report the game's current mouse-lock mode to the editor when
        // it changes (input.mouseLock), so the panel can capture/release the cursor to match.
        void report_mouse_lock();

      private:
        RpcServer _server = {};
        std::vector<std::unique_ptr<ViewStream>> _views = {};
        // Render textures of stopped views awaiting shared-surface teardown on the render lane.
        std::vector<tbx::RenderTexture> _pending_shared_destroys = {};
        mutable std::mutex _views_mutex = {};
        uint32 _next_view_index = 0U;
        std::weak_ptr<tbx::WorldManager> _world_manager = {};
        std::weak_ptr<tbx::Rendering> _rendering = {};
        std::weak_ptr<tbx::AssetManager> _asset_manager = {};
        std::weak_ptr<tbx::IInputManager> _input_manager = {};

        // Editor/game view cameras live here, separate from the game world, so the world holds only the
        // user's scene. This registry is updated and rendered by the plugin every frame regardless of
        // play state, and is never touched by the play-mode world snapshot.
        tbx::EntityRegistry _view_registry = {};

        // Deep copy of the game world captured when play begins; replayed to restore on stop. Split by
        // persistence so each entity is restored as runtime or global exactly as it was.
        tbx::EntityRegistry _runtime_snapshot = {};
        tbx::EntityRegistry _global_snapshot = {};

        // Borrowed from the Application for the plugin's own render calls; valid for the app lifetime.
        const tbx::GraphicsSettings* _graphics_settings = nullptr;

        std::string _app_name = {};
        uint16 _port = 0U;
        uint _log_listener_id = 0U;
        bool _had_client = false;
        bool _is_playing = false;
        // Last mouse-lock mode pushed to the editor; only changes are sent (input.mouseLock).
        tbx::MouseLockMode _last_reported_lock = tbx::MouseLockMode::UNLOCKED;
    };
}
