#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/systems/graphics/external_camera.h"
#include "tbx/types/assets/world.h"
#include "tbx/types/render_texture.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The cross-process shared-surface lifecycle of one view. Pending until the first present
    /// attempts to create the surface, then Ready (created + handle announced) → Presented (first real
    /// frame drawn + announced), or Unavailable when GPU sharing isn't supported (the editor shows its
    /// empty ghost).
    enum class ViewSurfaceState
    {
        Pending,
        Unavailable,
        Ready,
        Presented,
    };

    /// @brief
    /// Purpose: One editor-facing render view — the shared state every view kind has. A camera
    /// (`view`) the engine renders as an ExternalCamera into a dedicated render texture that is a
    /// shared GPU surface the editor samples directly (zero copy, no readback). Derived structs add
    /// only what their kind needs (EditorViewStream / GameViewStream / AssetPreviewViewStream). The
    /// forwarded viewport input lives off the stream, in the ViewManager's per-view ViewInput.
    /// @details
    /// Ownership: Owns its render texture + the id of the external camera it registered with the engine.
    /// The shared GPU texture itself is owned by the graphics backend (keyed by texture id) and torn
    /// down on the render lane. Thread Safety: mutated on the main thread under the view-manager lock;
    /// the surface fields are also read on the render lane (present callback) under that lock.
    struct ViewStream
    {
        virtual ~ViewStream() = default;

        std::string name = {};
        tbx::RenderTexture texture = {};
        // The camera the engine renders for this view (pose + lens + tags). Updated each frame from the
        // view's forwarded input (editor/preview) or mirrored from the game camera (game), then pushed
        // to the engine's external-camera registry. Editor views carry the editor-camera tag so the
        // gizmo / collider / selection passes apply.
        tbx::CameraView view = {};
        tbx::ExternalCameraId external_camera_id = {};

        // The view's shared GPU surface and where it is in its lifecycle (see ViewSurfaceState).
        tbx::SharedTargetInfo shared = {};
        ViewSurfaceState surface_state = ViewSurfaceState::Pending;
    };

    /// @brief
    /// Purpose: An editor view — a free fly camera over the active world.
    struct EditorViewStream : ViewStream
    {
        // The editor camera aims at the world once, after its geometry has streamed in (the world loads
        // a few frames after the view starts).
        bool needs_orient = true;
    };

    /// @brief
    /// Purpose: A game view — mirrors the live game camera; while playing, its forwarded input (held in
    /// the ViewManager's ViewInput) is fed into the engine input system.
    struct GameViewStream : ViewStream
    {
    };

    /// @brief
    /// Purpose: An asset-preview view — orbits an isolated world that holds a single previewed asset.
    struct AssetPreviewViewStream : ViewStream
    {
        // The isolated world holding the previewed asset's entity, owned here so it lives exactly as
        // long as the view and is rendered (as the external camera's world override) in place of the
        // active world.
        std::shared_ptr<tbx::World> preview_world = {};

        // A stable, non-zero numeric id the editor uses to target this preview world for world-level
        // ops (world.describe / entity.create). World id 0 is reserved for the active editing world;
        // per-entity ops resolve the owning world from the entity id instead.
        uint32 world_id = 0U;

        // The registered asset this preview shows (the editor builds + configures the previewed entity in
        // this world through the world/entity API; the bridge only seeds the shared light + sky assets).
        uint32 preview_asset_id = 0U;

        // Orbit-camera state: the camera sits at orbit_target + spherical(orbit_yaw, orbit_pitch) *
        // orbit_distance, always facing the target. Seeded from the asset's bounds when the view starts.
        tbx::Vec3 orbit_target = tbx::Vec3(0.0F);
        float orbit_yaw = 0.0F;
        float orbit_pitch = 0.3F;
        float orbit_distance = 3.0F;

        // Turntable: when set, the camera slowly auto-orbits the asset while no drag is in progress, so a
        // preview shows the model rotating on its own. A held button pauses it; it resumes on release. Off by
        // default (the dockable Asset Viewer stays still); the editor opts in per view (e.g. the hover card).
        bool auto_orbit = false;
    };
}
