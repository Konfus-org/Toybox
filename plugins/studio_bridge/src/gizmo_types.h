#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief Which world axis a handle acts on. ALL is the uniform-scale centre handle; VIEW is the
    /// camera-facing axis (screen-space) for the outer rotate ring, derived per frame from the camera.
    enum class GizmoAxis
    {
        NONE,
        X,
        Y,
        Z,
        ALL,
        VIEW,
    };

    /// @brief A transform handle's interaction semantics — the only gizmo vocabulary the engine keeps.
    /// What a handle LOOKS like is editor-authored (its op stream); what dragging it DOES is the kind.
    enum class GizmoHandleKind
    {
        ARROW,       // dragging translates along the axis
        RING,        // dragging rotates about the axis
        KNOB,        // dragging scales the axis
        CENTER,      // dragging scales uniformly on every axis
        PLANE,       // dragging translates in the plane whose normal is the axis (two axes at once)
        PLANE_SCALE, // dragging scales the two axes in the plane whose normal is the axis
    };

    /// @brief One editor-pushed transform handle (view.setGizmo). kind + axis define the drag
    /// semantics and the analytic hit shape; extent is the handle's reach in gizmo units (a unit
    /// space anchored at the pivot and scaled to the constant screen size); ops is the handle's
    /// editor-authored look — unit-space drawing ops replayed under the pivot/size matrix.
    struct GizmoHandle
    {
        GizmoHandleKind kind = GizmoHandleKind::ARROW;
        GizmoAxis axis = GizmoAxis::NONE;
        float extent = 1.0F;
        tbx::Json ops = tbx::Json::array();
    };

    /// @brief The editor-pushed snapping settings (view.setGizmo). The effective per-frame snap is
    /// `enabled` XOR "a snap-hold key is held": the toolbar toggle makes snapping the default and the
    /// held key momentarily inverts it. The hold keys arrive from the editor's keybinding system as
    /// tbx::InputKey codes — the engine hardcodes none.
    struct GizmoSnap
    {
        bool enabled = false;
        float translate = 0.5F;
        float rotate_deg = 15.0F;
        float scale = 0.1F;
        std::vector<int> keys = {};
    };

    /// @brief One selected entity captured at drag start, so each frame re-derives its transform from the
    /// anchor + current cursor (robust to the ~1-step input latency; no drift).
    struct GizmoDragTarget
    {
        tbx::Uuid id = {};
        tbx::Transform start_world = {};
        tbx::Transform start_local = {};
    };

    /// @brief Per-view transform-gizmo interaction state (hover + in-progress drag). Owned by the
    /// GizmoControllerState, keyed by view; the cursor itself lives on the view stream (forwarded input).
    struct GizmoState
    {
        bool left_was_down = false;

        // Indices into the active handle set (-1 = none); cleared whenever the set is replaced.
        int hovered = -1;
        int active = -1;
        bool dragging = false;

        // Drag anchor (world space), captured on the press that began the drag.
        tbx::Vec3 pivot = tbx::Vec3(0.0F);
        tbx::Vec3 axis_dir = tbx::Vec3(0.0F);
        tbx::Quat basis = tbx::Quat(1.0F, 0.0F, 0.0F, 0.0F); // gizmo orientation at drag start (identity = global)
        float start_param = 0.0F;                 // arrow/knob: closest-point param along the axis
        tbx::Vec3 start_vector = tbx::Vec3(0.0F); // ring: pivot -> first ring-plane hit
        float drag_size = 1.0F;                   // gizmo world size captured at drag start (scale reference)
        float drag_angle = 0.0F;                  // ring: current signed sweep, for the drag-amount arc
        std::vector<GizmoDragTarget> targets = {};
    };
}
