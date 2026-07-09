#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct GizmoControllerState;
    struct GizmoLayerState;

    /// @brief Registers the gizmo editor RPC methods: the transform-tool mode served by the
    /// gizmo ops, and the editor-authored overlay layers served by the gizmo-layer ops.
    void register_gizmo_handlers(
        const RpcRegistrar& registrar, GizmoControllerState& gizmos, GizmoLayerState& layers);
}
