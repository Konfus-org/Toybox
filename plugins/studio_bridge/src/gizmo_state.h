#pragma once
#include "gizmo_types.h"
#include "tbx/types/uuid.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The editor's transform-gizmo state — its own thing: the editor-pushed handle set
    /// (what the active tool is made of), the snap settings, the per-view interaction state
    /// (hover/drag), and the gizmo overlay pass id. Plain state: gizmo_ops owns the behavior
    /// (pass lifecycle, view.setGizmo, per-frame hit-test/drag + overlay submission). The handles'
    /// look is editor-authored (drawing ops); the engine only knows each handle's kind (its drag
    /// semantics + analytic hit shape) and snapping.
    /// @details
    /// Ownership: Owned by the plugin by value; the overlay pass draws the shared Gizmos service's
    /// batch, so no batch is owned here. Thread Safety: Main-thread only (driven from on_update);
    /// the pass's execute runs on the render lane.
    struct GizmoControllerState
    {
        // The active tool's handles, pushed by the editor's toolbar (view.setGizmo). Empty = no gizmo.
        std::vector<GizmoHandle> handles = {};

        // The editor-pushed snap settings; the per-frame effective snap XORs `enabled` with the hold keys.
        GizmoSnap snap = {};

        // Per-view interaction state (hover + in-progress drag), keyed by view name — owned here rather
        // than on the view stream so the gizmo is fully its own thing.
        std::unordered_map<std::string, GizmoState> view_states = {};

        // Id of the gizmo overlay pass on the engine Rendering service. Invalid until registered.
        tbx::Uuid pass = {};
    };
}
