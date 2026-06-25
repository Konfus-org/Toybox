#pragma once
#include "gizmo_types.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <memory>
#include <string>
#include <vector>

namespace tbx
{
    class World;
}

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: What a view renders and how its camera is driven. Editor views fly a free camera over
    /// the active world; Game views mirror the live game camera; AssetPreview views orbit an isolated
    /// preview world that holds a single asset (a texture on a quad, a material on a sphere, a model).
    enum class ViewKind
    {
        Editor,
        Game,
        AssetPreview,
    };

    /// @brief
    /// Purpose: One editor-facing render view — a camera drawing into a dedicated render texture
    /// that is a shared GPU surface the editor samples directly (zero copy, no readback).
    /// @details
    /// Ownership: Owns the editor camera entity it spawned. The shared GPU texture itself is owned
    /// by the graphics backend (keyed by texture id) and torn down on the render lane. Game views
    /// mirror the game camera every frame; AssetPreview views own their isolated preview world.
    struct ViewStream
    {
        std::string name = {};
        tbx::RenderTexture texture = {};
        tbx::Uuid camera_id = {};
        ViewKind kind = ViewKind::Editor;

        // For AssetPreview views: the isolated world holding the previewed asset's entity. Owned here
        // so it lives exactly as long as the view, and rendered/picked in place of the active world.
        // Null for Editor/Game views.
        std::shared_ptr<tbx::World> preview_world = {};

        // The registered asset this preview shows, kept so the preview can be rebuilt with different
        // options without restarting the view. preview_option is the presentation (mesh for a
        // material/texture, material for a model); preview_skybox is the background (day/night/none).
        // Both are remembered so changing one rebuilds with the other preserved.
        uint32 preview_asset_id = 0U;
        std::string preview_option = {};
        std::string preview_skybox = {};

        // Orbit-camera state for AssetPreview views (ignored otherwise): the camera sits at
        // orbit_target + spherical(orbit_yaw, orbit_pitch) * orbit_distance, always facing the target.
        // Seeded from the asset's bounds when the view starts.
        tbx::Vec3 orbit_target = tbx::Vec3(0.0F);
        float orbit_yaw = 0.0F;
        float orbit_pitch = 0.3F;
        float orbit_distance = 3.0F;

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
        // Editor cameras aim at the world once, after its geometry has streamed in (the world loads
        // a few frames after the view starts).
        bool needs_orient = true;

        // Raw game input forwarded for a game view (fed into the engine input system while playing and
        // this view is focused): pressed tbx::InputKey codes plus the absolute mouse position in the view.
        std::vector<int> keys = {};
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;

        // Transform-gizmo interaction state for this view.
        GizmoState gizmo = {};
    };
}
