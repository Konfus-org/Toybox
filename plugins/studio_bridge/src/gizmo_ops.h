#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/systems/time/delta_time.h"
#include <string>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct GizmoControllerState;
    struct SelectionState;
    struct ViewState;

    /// @brief Registers the transform-gizmo overlay pass on the engine Rendering service (gated on
    /// the editor-camera tag); it draws the shared Gizmos batch submit_gizmo_overlay fills each
    /// frame. Call once the rendering + gizmo services are resolved.
    void register_gizmo_pass(GizmoControllerState& state, const EngineServices& services);

    /// @brief Removes the gizmo overlay pass before the plugin unloads.
    void unregister_gizmo_pass(GizmoControllerState& state, const EngineServices& services);

    /// @brief view.setGizmo: replaces the handle set from { handles: [...] } and folds in
    /// { snap } when present.
    void set_gizmo(GizmoControllerState& state, const tbx::Json& params);

    /// @brief Whether the view's cursor is on a gizmo handle (hovered or mid-drag) — the pick
    /// handler's suppression query, so a tap on a handle never counts as a scene pick.
    bool is_cursor_on_gizmo(const GizmoControllerState& state, const std::string& view);

    /// @brief Per-frame hover hit-test + drag for the focused editor view; writes the resulting
    /// transform onto every selected entity and notifies the editor when a drag ends.
    void update_gizmos(
        GizmoControllerState& state,
        const SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const tbx::DeltaTime& dt);

    /// @brief Submits the active transform gizmo's handles for the focused (or first) editor view to
    /// the shared Gizmos batch. World-space, so one submission draws in every editor view.
    void submit_gizmo_overlay(
        GizmoControllerState& state,
        const SelectionState& selection,
        const EngineServices& services,
        ViewState& views);
}
