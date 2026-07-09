#include "gizmo_rpc_handlers.h"
#include "gizmo_layer_ops.h"
#include "gizmo_ops.h"
#include "gizmo_layer_state.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_gizmo_handlers(
        const RpcRegistrar& registrar, GizmoControllerState& gizmos, GizmoLayerState& layers)
    {
        registrar.add(
            Wire::VIEW_SET_GIZMO,
            [&gizmos](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor's transform-tool toolbar; no response.
                set_gizmo(gizmos, params);
            });

        // The editor-authored overlay layers are commands (the editor's sync pushes await the reply),
        // so they register as queries; the reply carries no body, only the outcome.
        registrar.add_query(
            Wire::GIZMOS_SET,
            [&layers](const tbx::Json& params, tbx::Json&) { return set_gizmo_layer(layers, params); });
        registrar.add_query(
            Wire::GIZMOS_REMOVE,
            [&layers](const tbx::Json& params, tbx::Json&)
            { return remove_gizmo_layer(layers, params); });
    }
}
