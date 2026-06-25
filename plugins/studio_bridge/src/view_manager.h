#pragma once
#include "engine_services.h"
#include "selection.h"
#include "view_stream.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Owns the editor's render views — their cameras (in a registry separate from the game
    /// world), shared GPU surfaces, and per-frame rendering. The single owner of the view collection
    /// and its mutex; other subsystems reach the views through with_views_locked / resolve_view_camera.
    /// @details
    /// Ownership: Owns _views, the view-camera registry, and the pending shared-surface destroy queue.
    /// Borrows the engine services (incl. the RPC host). Thread Safety: The view collection is guarded
    /// by an internal mutex; surface lifecycle runs on the render lane (ensure_view_surfaces), the rest
    /// on the main thread.
    class ViewManager
    {
      public:
        explicit ViewManager(EngineServices& services);

        // Submits the editor overlay (selection highlight + the focused view's transform handles) for
        // the focused editor view, called once per frame under the views lock with that view's camera
        // and gizmo state (both null when no editor view is focused).
        using SubmitOverlayFn =
            std::function<void(const tbx::CameraView* focused_camera, const GizmoState* focused_gizmo)>;

        // Starts a view of the given kind. For AssetPreview, asset_id selects the asset to load into
        // the view's isolated preview world (ignored for Editor/Game). Fails when the kind needs a
        // world that is unavailable, or when an asset preview cannot be built for the id.
        Result start_view(ViewKind kind, uint32 asset_id, std::string& out_name);
        void stop_view(const std::string& name);
        void stop_all_views();

        // Applies a view.input notification to its target view (focus, buttons, move keys, mouse/wheel
        // deltas, raw game keys, and the normalized gizmo cursor).
        void apply_view_input(const tbx::Json& params);

        // Rebuilds an asset-preview view with a different presentation option (e.g. show a material on
        // a cube, or as a skybox; a model under a different built-in material), preserving the chosen
        // background sky and the camera/orbit. Fails for an unknown or non-asset-preview view.
        Result set_preview_option(const std::string& view_name, const std::string& option);

        // Rebuilds an asset-preview view with a different background sky (day/night/none), preserving
        // the presentation option and the camera/orbit. Fails for an unknown or non-asset-preview view.
        Result set_preview_skybox(const std::string& view_name, const std::string& skybox);

        // Mirrors the live game camera's pose + lens onto every game view's mirror camera each frame.
        void sync_game_views();

        // Submits the editor overlay (via submit_overlay) then renders every view camera into its
        // texture; the engine's own loop only draws the game world's cameras.
        void render_views(const tbx::DeltaTime& dt, const SubmitOverlayFn& submit_overlay);

        // Render-lane callback body: frees stopped views' shared surfaces, then creates the rendering
        // view's surface on first use and announces its cross-process handle to the editor.
        void ensure_view_surfaces(
            tbx::IGraphicsBackend& backend,
            const tbx::RenderTarget& output_target,
            const tbx::Size& backbuffer_size);

        // Resolves a view's camera into a CameraView (for picking). Fails for an unknown/invalid view.
        Result resolve_view_camera(const std::string& view_name, tbx::CameraView& out_camera);

        // The world a view draws and picks against: an AssetPreview view's isolated preview world,
        // otherwise the active world. Null when the view is unknown or its world is unavailable.
        std::shared_ptr<tbx::World> resolve_view_world(const std::string& view_name) const;

        // The first AssetPreview preview world that contains the given entity id, or null when none do.
        // Lets entity resolution (describe/reflect) reach entities that live in a preview world rather
        // than the active world.
        std::shared_ptr<tbx::World> find_preview_world_with(const tbx::Uuid& id) const;

        // Runs fn(views, view_registry) under the views lock, so input/gizmo subsystems can read and
        // mutate the existing view cameras without owning the collection.
        template <typename Fn>
        void with_views_locked(Fn&& fn)
        {
            auto lock = std::lock_guard(_views_mutex);
            fn(_views, _view_registry);
        }

      private:
        // Rebuilds an asset-preview view's world from its stored asset id + option + skybox, hot-
        // swapping it in so the orbit camera is preserved. Shared by set_preview_option/skybox.
        Result rebuild_preview(const std::string& view_name);

        void refresh_present_callback();
        tbx::Entity find_first_game_camera(tbx::World& world) const;
        tbx::Uuid create_view_camera(tbx::World& world, const tbx::RenderTexture& texture, ViewKind kind);
        void destroy_view_camera(const tbx::Uuid& camera_id);

        // Creates the editor's selection-outline post effect on a runtime entity in the view registry.
        // The renderer processes these effects only for the editor views (passed to render_views), so
        // selection outlining is entirely editor-owned — the engine knows nothing about it.
        void create_selection_overlay();
        // The editor-only post-processing effects to overlay on editor views (empty if none).
        std::vector<tbx::PostProcessingEffect> editor_overlay_effects() const;

        // The RPC port (from the host), woven into each view/texture name so concurrent editors stay
        // distinct. 0 until the host is bound + listening.
        uint16 rpc_port() const
        {
            const auto host = _services.rpc_host.lock();
            return host ? host->port() : 0U;
        }

        EngineServices& _services;
        uint32 _next_view_index = 0U;

        std::vector<std::unique_ptr<ViewStream>> _views = {};
        // Render textures of stopped views awaiting shared-surface teardown on the render lane.
        std::vector<tbx::RenderTexture> _pending_shared_destroys = {};
        mutable std::mutex _views_mutex = {};

        // Editor/game view cameras live here, separate from the game world, so the world holds only the
        // user's entities. Updated and rendered every frame regardless of play state, and never touched by
        // the play-mode world snapshot.
        tbx::EntityRegistry _view_registry = {};

        // Runtime entity in _view_registry holding the editor's selection-outline post effect. Kept out
        // of the game world (and thus saves/play snapshots); its effects are overlaid on editor views.
        tbx::Uuid _overlay_entity = {};
    };
}
