#include "world_rpc_handlers.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "world_manager.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_world_handlers(const RpcRegistrar& registrar, WorldManager& world_manager)
    {
        registrar.add(
            Wire::WORLD_DESCRIBE,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.result(world_manager.describe_world(params));
            });
        registrar.add(
            Wire::WORLD_SAVE,
            [&world_manager](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.respond(world_manager.save_world());
            });
        registrar.add(
            Wire::WORLD_OPEN,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                // Opens a world/chunk asset as the active editing world (replacing the current one).
                r.respond(world_manager.open_world(params));
            });
        registrar.add(
            Wire::APP_DESCRIBE_SETTINGS,
            [&world_manager](const tbx::Json&, tbx::RpcResponder& r)
            {
                r.result(world_manager.describe_settings());
            });
    }
}
