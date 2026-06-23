#pragma once
#include "tbx/types/components/transform.h"
#include "tbx/types/uuid.h"
#include "tbx/types/vectors.h"
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief The active transform tool. NONE is the select tool (no gizmo; marquee box-select).
    enum class GizmoMode
    {
        NONE,
        TRANSLATE,
        ROTATE,
        SCALE,
    };

    /// @brief Which axis of the gizmo a value refers to (0 = none). ALL is the scale gizmo's centre
    /// handle, which scales uniformly on every axis.
    enum class GizmoAxis
    {
        NONE,
        X,
        Y,
        Z,
        ALL,
    };

    /// @brief One selected entity captured at drag start, so each frame re-derives its transform from the
    /// anchor + current cursor (robust to the ~1-step input latency; no drift).
    struct GizmoDragTarget
    {
        tbx::Uuid id = {};
        tbx::Transform start_world = {};
        tbx::Transform start_local = {};
    };

    /// @brief Per-view transform-gizmo interaction state (hover + in-progress drag).
    struct GizmoState
    {
        // Normalized cursor in the view's rendered image (top-left origin), forwarded each frame.
        float cursor_u = 0.0F;
        float cursor_v = 0.0F;
        bool left_was_down = false;

        GizmoAxis hovered_axis = GizmoAxis::NONE;
        GizmoAxis active_axis = GizmoAxis::NONE;
        bool dragging = false;

        // Drag anchor (world space), captured on the press that began the drag.
        tbx::Vec3 pivot = tbx::Vec3(0.0F);
        tbx::Vec3 axis_dir = tbx::Vec3(0.0F);
        float start_param = 0.0F;                 // translate/scale: closest-point param along the axis
        tbx::Vec3 start_vector = tbx::Vec3(0.0F); // rotate: pivot -> first ring-plane hit
        float drag_size = 1.0F;                   // gizmo world size captured at drag start (scale reference)
        float drag_angle = 0.0F;                  // rotate: current signed sweep, for the drag-amount arc
        std::vector<GizmoDragTarget> targets = {};
    };
}
