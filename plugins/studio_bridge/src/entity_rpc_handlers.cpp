#include "entity_rpc_handlers.h"
#include "engine_services.h"
#include "rpc_registrar.h"
#include "view_state.h"
#include "wire.h"
#include "world_ops.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_entity_handlers(
        const RpcRegistrar& registrar, const EngineServices& services, ViewState& views)
    {
        registrar.add(
            Wire::ENTITY_ADD_COMPONENT,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(add_component(services, views, params));
            });
        registrar.add(
            Wire::ENTITY_REMOVE_COMPONENT,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(remove_component(services, views, params));
            });
        registrar.add(
            Wire::ENTITY_ADD_SCRIPT,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(add_script(services, views, params));
            });
        registrar.add(
            Wire::ENTITY_REMOVE_SCRIPT,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(remove_script(services, views, params));
            });
        registrar.add_query(
            Wire::ENTITY_DESCRIBE,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                return describe_entity(services, views, params, reply);
            });
        registrar.add_query(
            Wire::ENTITY_CREATE,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                return create_entity(services, views, params, reply);
            });
        registrar.add(
            Wire::ENTITY_DUPLICATE,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(duplicate_entity(services, views, params));
            });
        registrar.add(
            Wire::ENTITY_DESTROY,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(destroy_entity(services, views, params));
            });
        // entity.setName/setGlobal/setEnabled/setTags are GONE: an entity's scalar fields are now edited
        // through the uniform sync.set { path } verb (world/{w}/entities/{id}/{field}), which routes to the
        // same set_entity_* implementations. Only the structural ops (create/destroy/move/component/script)
        // keep their own verbs, as they change shape rather than a field's value.
        registrar.add(
            Wire::ENTITY_MOVE,
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(move_entity(services, views, params));
            });
    }
}
