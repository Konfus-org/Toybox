#include "sync_rpc_handlers.h"
#include "engine_services.h"
#include "game_mode_state.h"
#include "physics_event_ops.h"
#include "rpc_registrar.h"
#include "sync_event_ops.h"
#include "sync_event_state.h"
#include "sync_path_ops.h"
#include "view_state.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    void register_sync_handlers(
        const RpcRegistrar& registrar,
        const EngineServices& services,
        ViewState& views,
        SyncEventState& events,
        const GameModeState& game_mode)
    {
        // The uniform address-addressed sync verbs: every tier (entity, component, asset, …) is one
        // { address } (see EngineAddress on the editor side) instead of a verb family per kind. describe
        // reads an object, set writes one of its fields (the leaf the address ends on). Default-tracking
        // and reset are the editor's own concern (it computes them from its C# mirror defaults), so there
        // are no reset/isDefault verbs.
        registrar.add_query(
            Wire::SYNC_DESCRIBE,
            [&services, &views](const tbx::Json& params, tbx::Json& reply)
            {
                return sync_describe_path(services, views, params, reply);
            });
        // The uniform sync.set plus Studio 2.0's family write verbs, which address a component-property
        // or entity-scalar edit by family: all carry the same world-qualified { address, value } payload,
        // so they share one path-addressed set implementation. A component edit — a gizmo drag's landed
        // Transform and, crucially, its undo restore — rides component.set; an entity scalar rides
        // entity.set. (asset.* body writes are their own verbs, not this path.)
        const auto set_handler =
            [&services, &views](const tbx::Json& params, tbx::RpcResponder& r)
        {
            r.respond(sync_set_path(services, views, params));
        };
        registrar.add(Wire::SYNC_SET, set_handler);
        registrar.add(Wire::COMPONENT_SET, set_handler);
        registrar.add(Wire::ENTITY_SET, set_handler);
        // The sync.event channel's subscription verbs: which (address, key) raises the editor wants
        // streamed back (see sync_event_ops).
        registrar.add(
            Wire::SYNC_SUBSCRIBE,
            [&events, &services, &game_mode](const tbx::Json& params, tbx::RpcResponder& r)
            {
                const auto result = subscribe_sync_event(events, params);
                // A physics-event subscription made mid-play must forward from this session; the
                // bind is idempotent, so re-scanning only attaches the newly-subscribed entity.
                // (While stopped there is nothing to attach to — play-start binds every subscription.)
                if (result && game_mode.is_playing)
                    bind_subscribed_physics_events(events, services);
                r.respond(result);
            });
        registrar.add(
            Wire::SYNC_UNSUBSCRIBE,
            [&events](const tbx::Json& params, tbx::RpcResponder& r)
            {
                r.respond(unsubscribe_sync_event(events, params));
            });
    }
}
