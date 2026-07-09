#include "world_rpc_handlers.h"
#include "engine_services.h"
#include "rpc_registrar.h"
#include "view_state.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_world_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views)
    {
        registrar.add(
            Wire::WORLD_DESCRIBE,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.result(describe_world(services, views, params));
            });
        registrar.add(
            Wire::WORLD_SAVE,
            [&services](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.respond(save_world(services));
            });
        registrar.add(
            Wire::WORLD_OPEN,
            [&services](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Opens a world/chunk asset as the active editing world (replacing the current one).
                r.respond(open_world(services, params));
            });
    }
}
