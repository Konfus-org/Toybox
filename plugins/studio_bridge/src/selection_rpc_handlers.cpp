#include "selection_rpc_handlers.h"
#include "engine_services.h"
#include "gizmo_ops.h"
#include "picking_ops.h"
#include "picking_state.h"
#include "rpc_registrar.h"
#include "selection_ops.h"
#include "selection_state.h"
#include "view_state.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include <string>

namespace tbx::studio_bridge
{
    void register_selection_handlers(
        const RpcRegistrar& registrar,
        PickingState& picking,
        SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const GizmoControllerState& gizmos)
    {
        registrar.add_query(
            Wire::VIEW_PICK,
            [&picking, &services, &views, &gizmos](const tbx::Json& params, tbx::Json& reply)
            {
                // A tap on a transform handle is gizmo intent, not a pick: flag it so the editor
                // neither selects nor clears (clearing would drop the gizmo mid-interaction).
                if (is_cursor_on_gizmo(gizmos, params.value(Wire::VIEW, std::string())))
                {
                    reply[Wire::ID] = nullptr;
                    reply[Wire::GIZMO] = true;
                    return tbx::Result::OK;
                }
                return pick(picking, services, views, params, reply);
            });
        registrar.add_query(
            Wire::VIEW_PICK_RECT,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                return pick_rect(services, views, params, reply);
            });
        registrar.add_query(
            Wire::VIEW_PROJECT_ENTITIES,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                // Where a view's entities land on screen (billboard overlay positions). The editor polls
                // this on its own cadence; the engine projects only when asked.
                return project_entities(services, views, params, reply);
            });
        registrar.add_query(
            Wire::VIEW_QUERY_OCCLUSION,
            [&picking, &services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                // Which of the given entities are hidden behind geometry from a view's camera (billboard
                // overlay visibility), batched into one scene pass.
                return query_occlusion(picking, services, views, params, reply);
            });
        registrar.add_query(
            Wire::SELECTION_SET,
            [&selection, &services, &views](const tbx::Json& params, tbx::Json&)
            {
                // The editor's engine-sync push ({ key: "ids", value: [...] }); its sync layer
                // awaits the reply, so this registers as a query with an outcome-only response.
                if (params.value(Wire::KEY, std::string()) != Wire::IDS)
                    return tbx::Result(false, "selection.set: unknown key.");
                const auto it = params.find(Wire::VALUE);
                apply_selection(
                    selection, services, views, it != params.end() ? *it : tbx::Json::array());
                return tbx::Result::OK;
            });
    }
}
