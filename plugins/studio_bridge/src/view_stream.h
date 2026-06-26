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
#include <vector>

namespace tbx::studio_bridge
{
    // Sentinel preview_skybox_id meaning "no override": keep the bundled preview world's authored sky
    // (the day sky), as opposed to 0 which means "no sky at all". The editor sends a real sky material
    // id (or 0) once the user picks from the skybox picker.
    inline constexpr uint32 PREVIEW_SKYBOX_DEFAULT = 0xFFFFFFFFU;

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
    /// only what their kind needs (EditorViewStream / GameViewStream / AssetPreviewViewStream).
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
        // The camera the engine renders for this view (pose + lens + tags). Updated each frame from
        // forwarded input (editor/preview) or mirrored from the game camera (game), then pushed to the
        // engine's external-camera registry. Editor views carry the editor-camera tag so the gizmo /
        // collider / selection passes apply.
        tbx::CameraView view = {};
        tbx::ExternalCameraId external_camera_id = {};

        // The view's shared GPU surface and where it is in its lifecycle (see ViewSurfaceState).
        tbx::SharedTargetInfo shared = {};
        ViewSurfaceState surface_state = ViewSurfaceState::Pending;

        // Forwarded input common to every view (from the editor's matching viewport, via view.input).
        // Buttons are the latest held state; the mouse/wheel deltas accumulate between engine frames and
        // are consumed when a view's camera applies them. cursor_u/v is the normalized cursor in the
        // rendered image (top-left origin), read by the gizmo.
        bool focused = false;
        uint32 buttons = 0U; // bit0 = left, bit1 = right, bit2 = middle
        float accumulated_mouse_dx = 0.0F;
        float accumulated_mouse_dy = 0.0F;
        float accumulated_wheel = 0.0F;
        float cursor_u = 0.0F;
        float cursor_v = 0.0F;
    };

    /// @brief
    /// Purpose: An editor view — a free fly camera over the active world.
    struct EditorViewStream : ViewStream
    {
        uint32 move_keys = 0U; // bit0 fwd, 1 back, 2 left, 3 right, 4 up, 5 down
        // The editor camera aims at the world once, after its geometry has streamed in (the world loads
        // a few frames after the view starts).
        bool needs_orient = true;
    };

    /// @brief
    /// Purpose: A game view — mirrors the live game camera and forwards raw input to the engine input
    /// system while playing.
    struct GameViewStream : ViewStream
    {
        // Raw game input fed into the engine input system while playing and this view is focused:
        // pressed tbx::InputKey codes plus the absolute mouse position in the view.
        std::vector<int> keys = {};
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;
    };

    /// @brief
    /// Purpose: An asset-preview view — orbits an isolated world that holds a single previewed asset.
    struct AssetPreviewViewStream : ViewStream
    {
        // The isolated world holding the previewed asset's entity, owned here so it lives exactly as
        // long as the view and is rendered (as the external camera's world override) in place of the
        // active world.
        std::shared_ptr<tbx::World> preview_world = {};

        // The registered asset this preview shows + the editor-driven presentation, kept so the preview
        // can be rebuilt with different options without restarting the view. preview_option is a built-in
        // mesh token (material/texture) or "skybox"/"skysphere"; preview_material_id is the built-in
        // surface material a model is shown under (0 = the model's own); preview_skybox_id is the built-in
        // sky material (0 = no sky, PREVIEW_SKYBOX_DEFAULT = the authored day sky).
        uint32 preview_asset_id = 0U;
        std::string preview_option = {};
        uint32 preview_material_id = 0U;
        uint32 preview_skybox_id = PREVIEW_SKYBOX_DEFAULT;

        // Orbit-camera state: the camera sits at orbit_target + spherical(orbit_yaw, orbit_pitch) *
        // orbit_distance, always facing the target. Seeded from the asset's bounds when the view starts.
        tbx::Vec3 orbit_target = tbx::Vec3(0.0F);
        float orbit_yaw = 0.0F;
        float orbit_pitch = 0.3F;
        float orbit_distance = 3.0F;
    };
}
