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
        registrar.add_query(
            Wire::WORLD_OPEN,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                // Opens a world/chunk asset into the main world: "replace" (default) swaps the active world;
                // "additive" loads on top of it and replies with a worldAssetId the editor later closes.
                return open_world(services, views, params, reply);
            });
        registrar.add_query(
            Wire::WORLD_LOAD,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                // Loads a world alongside the active one; replies with its stable worldId.
                return load_world(services, views, params, reply);
            });
        registrar.add(
            Wire::WORLD_CLOSE,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(close_world(services, views, params));
            });
    }
}
