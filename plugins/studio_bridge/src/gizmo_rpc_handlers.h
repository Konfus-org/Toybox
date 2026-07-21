#pragma once

namespace tbx::studio_bridge
{
    class RpcRegistrar;
    struct GizmoControllerState;

    /// @brief Registers the gizmo editor RPC methods: the transform-tool mode served by the gizmo
    /// ops. (The editor's overlay drawing rides the data plane's draw lane, not RPC.)
    void register_gizmo_handlers(const RpcRegistrar& registrar, GizmoControllerState& gizmos);
}
