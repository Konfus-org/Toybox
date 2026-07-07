#include "gizmo_rpc_handlers.h"
#include "gizmo_controller.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_gizmo_handlers(const RpcRegistrar& registrar, GizmoController& gizmos)
    {
        registrar.add(
            Wire::VIEW_SET_GIZMO,
            [&gizmos](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor's transform-tool toolbar; no response.
                gizmos.set_mode(params);
            });
    }
}
