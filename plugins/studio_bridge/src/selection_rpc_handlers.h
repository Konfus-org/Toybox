#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct EngineServices;
    struct GizmoControllerState;
    struct PickingState;
    struct SelectionState;
    struct ViewState;

    /// @brief Registers the picking + selection editor RPC methods, served by the picking_ops and
    /// selection_ops free functions. The gizmo state is the pick suppressor: a tap on a
    /// transform handle is gizmo intent, not a scene pick.
    void register_selection_handlers(
        const RpcRegistrar& registrar,
        PickingState& picking,
        SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const GizmoControllerState& gizmos);
}
