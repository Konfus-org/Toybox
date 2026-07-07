#include "sync_rpc_handlers.h"
#include "rpc_registrar.h"
#include "sync_path_router.h"
#include "wire.h"
#include "world_manager.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_sync_handlers(
        const RpcRegistrar& registrar, SyncPathRouter& sync_router, WorldManager& world_manager)
    {
        registrar.add(
            Wire::SYNC_CATALOG,
            [&world_manager](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(world_manager.sync_catalog());
            });
        // The uniform path-addressed sync verbs: every tier (entity, component, …) is one { path }
        // (see EngineAddress on the editor side) instead of a verb family per kind. describe reads an object,
        // set/reset/isDefault write one of its fields (the leaf the path ends on).
        registrar.add_query(
            Wire::SYNC_DESCRIBE,
            [&sync_router](const tbx::Json& params, tbx::Json& reply)
            {
                return sync_router.sync_describe_path(params, reply);
            });
        registrar.add(
            Wire::SYNC_SET,
            [&sync_router](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(sync_router.sync_set_path(params));
            });
        registrar.add(
            Wire::SYNC_RESET,
            [&sync_router](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(sync_router.sync_reset_path(params));
            });
        registrar.add(
            Wire::SYNC_IS_DEFAULT,
            [&sync_router](const tbx::Json& params, tbx::RpcResponder& r)
            {
                auto is_default = false;
                const auto result = sync_router.sync_is_default_path(params, is_default);
                if (result)
                {
                    auto reply = tbx::Json::object();
                    reply[Wire::IS_DEFAULT_REPLY] = is_default;
                    r.result(reply);
                }
                else
                {
                    r.respond(result);
                }
            });
    }
}
