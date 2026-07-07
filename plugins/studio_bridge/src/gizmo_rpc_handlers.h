#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    class GizmoController;

    /// @brief Registers the transform-gizmo editor RPC methods served by the GizmoController.
    void register_gizmo_handlers(const RpcRegistrar& registrar, GizmoController& gizmos);
}
