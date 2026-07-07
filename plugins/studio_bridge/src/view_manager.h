#pragma once
#include "engine_services.h"
#include "view_input.h"
#include "view_stream.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Owns the editor's render views — the view collection, each view's camera (registered
    /// with the engine as an ExternalCamera, which the engine renders), and the cross-process shared
    /// GPU surface lifecycle. It no longer renders: the engine renders the external cameras and the
    /// gizmo/collider/selection passes draw the overlays. Other subsystems reach the views through
    /// with_views_locked / resolve_view_camera.
    /// @details
    /// Ownership: Owns _views and the pending shared-surface destroy queue; registers each view's
    /// external camera with the engine. Holds a non-owning reference to the engine services. Thread
    /// Safety: The view collection is guarded by an internal mutex; the surface lifecycle runs on the
    /// render lane (ensure_view_surfaces) under that lock, the rest on the main thread.
    class ViewManager
    {
      public:
        explicit ViewManager(EngineServices& services);

      public:
        /// @brief Starts an editor view (a free fly camera over the active world). Returns its name.
        Result start_editor_view(std::string& out_name);

        /// @brief Starts a game view (mirrors the live game camera). Returns its name.
        Result start_game_view(std::string& out_name);

        /// @brief Starts an asset-preview view orbiting an isolated world that holds the given asset.
        /// Returns its name and the isolated world's stable numeric id (which the editor targets for
        /// world-level ops). @p turntable auto-orbits the camera; @p render_scale (0..1] scales the view's
        /// resolution down for a cheaper preview. Fails when the asset cannot be previewed.
        Result start_asset_preview_view(
            uint32 asset_id, bool turntable, float render_scale, std::string& out_name, uint32& out_world_id);

        /// @brief Stops the named view: unregisters its external camera and queues its shared surface
        /// for teardown.
        void stop_view(const std::string& name);

        /// @brief Stops every view (editor disconnect / plugin teardown).
        void stop_all_views();

        /// @brief Applies a view.input notification to its target view (focus, buttons, mouse/wheel
        /// deltas, the gizmo cursor, plus editor move-keys / raw game keys + mouse position).
        void apply_view_input(const tbx::Json& params);

        /// @brief Frames the orbit camera of the asset-preview world with the given id to the renderable
        /// bounds the editor built in it (the previewed entity), or a sensible default when nothing has
        /// bounds yet. The editor calls this after creating/swapping the previewed entity. Fails for an
        /// unknown preview world.
        Result frame_asset_preview(uint32 world_id);

        /// @brief Mirrors the live game camera's pose + lens onto every game view's camera each frame.
        void sync_game_cameras();

        /// @brief Pushes each view's current camera (pose + lens + world) to the engine's
        /// external-camera registry. Call once per frame after the cameras are updated; the engine
        /// renders the registered cameras.
        void push_external_cameras();

        /// @brief Render-lane present callback: frees stopped views' shared surfaces, creates a view's
        /// surface on first present, and announces its cross-process handle (then its first frame) to
        /// the editor.
        void ensure_view_surfaces(
            tbx::IGraphicsBackend& backend,
            const tbx::RenderTarget& output_target,
            const tbx::Size& backbuffer_size);

        /// @brief Returns a view's camera (for picking / projection). Fails for an unknown/invalid view.
        Result resolve_view_camera(const std::string& view_name, tbx::CameraView& out_camera) const;

        /// @brief The world a view draws and picks against: an asset-preview view's isolated world,
        /// otherwise the active world. Null when the view is unknown or its world is unavailable.
        std::shared_ptr<tbx::World> resolve_view_world(const std::string& view_name) const;

        /// @brief The asset-preview world with the given stable numeric id, or null when none matches
        /// (the caller falls back to the active world). World id 0 never matches a preview world.
        std::shared_ptr<tbx::World> resolve_world_by_id(uint32 world_id) const;

        /// @brief The first asset-preview world that contains the given entity id, or null when none do.
        std::shared_ptr<tbx::World> find_preview_world_with(const tbx::Uuid& id) const;

        /// @brief Runs fn(views, inputs) under the views lock, so the input/gizmo subsystems can read and
        /// mutate the existing view cameras and their forwarded input (keyed by view name) without owning
        /// the collection.
        template <typename Fn>
        void with_views_locked(Fn&& fn)
        {
            auto lock = std::lock_guard(_views_mutex);
            fn(_views, _view_inputs);
        }

      private:
        // Builds an initial camera view for a new view's texture. Editor/game views open aligned with
        // the active world's game camera (game views also copy its lens); preview views open at the
        // origin (the orbit camera takes over). Editor views carry the editor-camera tag so the gizmo /
        // collider / selection passes apply.
        tbx::CameraView seed_camera_view(
            const tbx::RenderTexture& texture,
            bool editor_tag,
            bool copy_game_lens,
            bool seed_pose) const;
        // Generates a unique view name + a render texture sized to the graphics resolution, optionally scaled
        // down (0 < scale < 1) for a cheaper low-resolution view such as the browser's small hover preview.
        std::pair<std::string, tbx::RenderTexture> make_view_target(float scale = 1.0F);
        // Registers the (type-built, camera-seeded) view's external camera with the engine and adds it
        // to the collection. world_override is the preview world (null = active world).
        void register_and_add(
            std::unique_ptr<ViewStream> view,
            const std::shared_ptr<tbx::World>& world_override);
        void refresh_present_callback();

        // The RPC port (from the host), woven into each view/texture name so concurrent editors stay
        // distinct. 0 until the host is bound + listening.
        uint16 rpc_port() const
        {
            const auto host = _services.get().rpc_host.lock();
            return host ? host->port() : 0U;
        }

      private:
        std::reference_wrapper<EngineServices> _services;
        uint32 _next_view_index = 0U;
        // Hands each new asset-preview world a unique non-zero id (0 is reserved for the active world).
        uint32 _next_world_id = 1U;

        std::vector<std::unique_ptr<ViewStream>> _views = {};
        // The forwarded input for each live view, keyed by view name — created and dropped with the view
        // so it never outlives its stream. Kept off the stream so a ViewStream is purely render state.
        std::unordered_map<std::string, ViewInput> _view_inputs = {};
        // Render textures of stopped views awaiting shared-surface teardown on the render lane.
        std::vector<tbx::RenderTexture> _pending_shared_destroys = {};
        mutable std::mutex _views_mutex = {};
    };
}
