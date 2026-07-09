#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/types/color.h"
#include "tbx/types/matrices.h"

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Replays one editor-authored gizmo drawing-op stream into a Gizmos batch. An op is a
    /// compact array whose head names the tbx::Gizmos method it replays into (["wire_box", [c], [s]])
    /// — the wire vocabulary IS the engine's method list, so the wire carries shapes, not vertices.
    /// Shared by the retained layer store (gizmos.set layers) and the transform-gizmo controller
    /// (handle visuals), so every op spelling parses in exactly one place.
    /// @details
    /// State: a replay starts from the engine defaults (white color, identity matrix) and leaves the
    /// batch reset, so one stream's trailing state never leaks into the next writer. `base_matrix`,
    /// when given, is the caller's space the op coordinates are authored in (e.g. the gizmo's
    /// pivot/size frame): it premultiplies every matrix op and anchors reset_matrix. `color_override`,
    /// when given, pins the draw color and mutes the stream's own color ops — the hover/active
    /// highlight tint.
    void replay_gizmo_ops(
        tbx::Gizmos& gizmos,
        const tbx::Json& ops,
        const tbx::Mat4* base_matrix = nullptr,
        const tbx::Color* color_override = nullptr);
}
