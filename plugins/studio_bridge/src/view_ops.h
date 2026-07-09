#pragma once
#include "view_state.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/utils/result.h"
#include <memory>
#include <mutex>
#include <string>

namespace tbx::studio_bridge
{
    struct EngineServices;

    // The render-view subsystem's behavior over the plugin's ViewState: view start/stop, camera
    // seeding/sync, the render-lane shared-surface lifecycle (present callback), and the
    // view-resolution queries every other domain calls.

    /// @brief Starts an editor view (a free fly camera over the active world). Returns its name.
    Result start_editor_view(ViewState& views, const EngineServices& services, std::string& out_name);

    /// @brief Starts a game view (mirrors the live game camera). Returns its name.
    Result start_game_view(ViewState& views, const EngineServices& services, std::string& out_name);

    /// @brief Starts an asset-preview view orbiting an isolated world that holds the given asset.
    /// Returns its name and the isolated world's stable numeric id (which the editor targets for
    /// world-level ops). @p turntable auto-orbits the camera; @p render_scale (0..1] scales the view's
    /// resolution down for a cheaper preview. Fails when the asset cannot be previewed.
    Result start_asset_preview_view(
        ViewState& views,
        const EngineServices& services,
        uint32 asset_id,
        bool turntable,
        float render_scale,
        std::string& out_name,
        uint32& out_world_id);

    /// @brief Stops the named view: unregisters its external camera and queues its shared surface
    /// for teardown.
    void stop_view(ViewState& views, const EngineServices& services, const std::string& name);

    /// @brief Stops every view (editor disconnect / plugin teardown).
    void stop_all_views(ViewState& views, const EngineServices& services);

    /// @brief Applies a view.input notification to its target view (focus, buttons, mouse/wheel
    /// deltas, the gizmo cursor, plus editor move-keys / raw game keys + mouse position).
    void apply_view_input(ViewState& views, const tbx::Json& params);

    /// @brief Frames the orbit camera of the asset-preview world with the given id to the renderable
    /// bounds the editor built in it (the previewed entity), or a sensible default when nothing has
    /// bounds yet. The editor calls this after creating/swapping the previewed entity. Fails for an
    /// unknown preview world.
    Result frame_asset_preview(ViewState& views, const EngineServices& services, uint32 world_id);

    /// @brief Mirrors the live game camera's pose + lens onto every game view's camera each frame.
    void sync_game_cameras(ViewState& views, const EngineServices& services);

    /// @brief Pushes each view's current camera (pose + lens + world) to the engine's
    /// external-camera registry. Call once per frame after the cameras are updated; the engine
    /// renders the registered cameras.
    void push_external_cameras(ViewState& views, const EngineServices& services);

    /// @brief Returns a view's camera (for picking / projection). Fails for an unknown/invalid view.
    Result resolve_view_camera(
        const ViewState& views, const std::string& view_name, tbx::CameraView& out_camera);

    /// @brief The world a view draws and picks against: an asset-preview view's isolated world,
    /// otherwise the active world. Null when the view is unknown or its world is unavailable.
    std::shared_ptr<tbx::World> resolve_view_world(
        const ViewState& views, const EngineServices& services, const std::string& view_name);

    /// @brief The asset-preview world with the given stable numeric id, or null when none matches
    /// (the caller falls back to the active world). World id 0 never matches a preview world.
    std::shared_ptr<tbx::World> resolve_world_by_id(const ViewState& views, uint32 world_id);

    /// @brief The first asset-preview world that contains the given entity id, or null when none do.
    std::shared_ptr<tbx::World> find_preview_world_with(const ViewState& views, const tbx::Uuid& id);

    /// @brief Runs fn(streams, inputs) under the views lock, so the input/gizmo subsystems can read and
    /// mutate the existing view cameras and their forwarded input (keyed by view name) without owning
    /// the collection.
    template <typename Fn>
    void with_views_locked(ViewState& views, Fn&& fn)
    {
        auto lock = std::lock_guard(views.mutex);
        fn(views.streams, views.inputs);
    }
}
