#include "entity_rpc_handlers.h"
#include "rpc_registrar.h"
#include "wire.h"
#include "world_manager.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_entity_handlers(const RpcRegistrar& registrar, WorldManager& world_manager)
    {
        registrar.add(
            Wire::ENTITY_SET_COMPONENT,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.apply_component(params));
            });
        registrar.add(
            Wire::ENTITY_ADD_COMPONENT,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.add_component(params));
            });
        registrar.add(
            Wire::ENTITY_REMOVE_COMPONENT,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.remove_component(params));
            });
        registrar.add(
            Wire::ENTITY_ADD_SCRIPT,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.add_script(params));
            });
        registrar.add_query(
            Wire::ENTITY_DESCRIBE,
            [&world_manager](const tbx::Json& params, tbx::Json& reply)
            {
                return world_manager.describe_entity(params, reply);
            });
        registrar.add_query(
            Wire::ENTITY_CREATE,
            [&world_manager](const tbx::Json& params, tbx::Json& reply)
            {
                return world_manager.create_entity(params, reply);
            });
        registrar.add(
            Wire::ENTITY_DESTROY,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.destroy_entity(params));
            });
        // entity.setName/setGlobal/setEnabled/setTags are GONE: an entity's scalar fields are now edited
        // through the uniform sync.set { path } verb (world/{w}/entities/{id}/{field}), which routes to the
        // same set_entity_* implementations. Only the structural ops (create/destroy/move/component/script)
        // keep their own verbs, as they change shape rather than a field's value.
        registrar.add(
            Wire::ENTITY_MOVE,
            [&world_manager](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(world_manager.move_entity(params));
            });
    }
}
