#pragma once

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct HoverState;
    struct PickingState;
    struct ViewState;

    /// @brief Refreshes the entity-under-cursor of the focused editor view: raycasts (only when the
    /// cursor or camera actually moved, and no mouse button is held — a drag keeps the current
    /// hover), swaps the editor.hovered runtime tag from the old entity to the new one, and sends
    /// the view.hover notification { view, id-or-null } on change. When no editor view is focused,
    /// the hover clears. Call once per on_update, after the cameras and gizmo have updated.
    void update_hover(
        HoverState& hover,
        PickingState& picking,
        const EngineServices& services,
        ViewState& views);
}
