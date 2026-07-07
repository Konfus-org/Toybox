#include "selection_rpc_handlers.h"
#include "engine_services.h"
#include "entity_selection_handler.h"
#include "rpc_registrar.h"
#include "selection.h"
#include "tags.h"
#include "view_manager.h"
#include "wire.h"
#include "tbx/interfaces/rpc_router.h"
#include "tbx/systems/files/json.h"
#include <memory>

namespace tbx::studio_bridge
{
    void register_selection_handlers(
        const RpcRegistrar& registrar,
        EntitySelectionHandler& selection_handler,
        Selection& selection,
        const EngineServices& services,
        ViewManager& views)
    {
        registrar.add_query(
            Wire::VIEW_PICK,
            [&selection_handler](const tbx::Json& params, tbx::Json& reply)
            {
                return selection_handler.pick(params, reply);
            });
        registrar.add_query(
            Wire::VIEW_PICK_RECT,
            [&selection_handler](const tbx::Json& params, tbx::Json& reply)
            {
                return selection_handler.pick_rect(params, reply);
            });
        registrar.add_query(
            Wire::VIEW_PROJECT_ENTITIES,
            [&selection_handler](const tbx::Json& params, tbx::Json& reply)
            {
                // Where a view's entities land on screen (billboard overlay positions). The editor polls
                // this on its own cadence; the engine projects only when asked.
                return selection_handler.project_entities(params, reply);
            });
        registrar.add_query(
            Wire::VIEW_QUERY_OCCLUSION,
            [&selection_handler](const tbx::Json& params, tbx::Json& reply)
            {
                // Which of the given entities are hidden behind geometry from a view's camera (billboard
                // overlay visibility), batched into one scene pass.
                return selection_handler.query_occlusion(params, reply);
            });
        registrar.add(
            Wire::VIEW_SET_SELECTION,
            [&selection, &services, &views](const tbx::Json& params, tbx::RpcResponder&)
            {
                // Notification from the editor whenever the selection changes; no response. The
                // selection is shown as an outline by the engine's tag-gated selection-outline post
                // effect, so mark the newly selected entities with the runtime selected tag and clear
                // it from the previously selected ones (a no-op on stale/absent ids). An id may live in
                // the active world or in an asset-preview view's world, so resolve each id's world.
                const auto world_of = [&services, &views](const tbx::Uuid& id) -> std::shared_ptr<tbx::World>
                {
                    if (auto world = services.active_world(); world && world->has(id))
                        return world;
                    return views.find_preview_world_with(id);
                };

                for (const auto& id : selection.ids())
                    if (auto world = world_of(id))
                        world->get(id).remove_tag(Tags::SELECTED);
                selection.set_from_params(params);
                for (const auto& id : selection.ids())
                    if (auto world = world_of(id))
                        world->get(id).add_tag(Tags::SELECTED, /*serialized*/ false);
            });
    }
}
