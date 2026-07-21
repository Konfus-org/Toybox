#include "connection_rpc_handlers.h"
#include "connection_ops.h"
#include "engine_services.h"
#include "rpc_registrar.h"
#include "view_state.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_connection_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views)
    {
        registrar.add(
            Wire::CONNECTION_ADD,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(add_connection(services, views, params));
            });
        registrar.add(
            Wire::CONNECTION_REMOVE,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(remove_connection(services, views, params));
            });
    }
}
