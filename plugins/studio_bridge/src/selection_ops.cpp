#include "selection_ops.h"
#include "engine_services.h"
#include "selection_state.h"
#include "tags.h"
#include "view_ops.h"
#include "tbx/types/typedefs.h"
#include <memory>
#include <utility>
#include <vector>

namespace tbx::studio_bridge
{
    void apply_selection(
        SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& ids_array)
    {
        const auto world_of = [&services, &views](const tbx::Uuid& id) -> std::shared_ptr<tbx::World>
        {
            if (auto world = services.active_world(); world && world->has(id))
                return world;
            return find_preview_world_with(views, id);
        };

        for (const auto& id : selection.ids)
            if (auto world = world_of(id))
                world->get(id).remove_tag(Tags::SELECTED);

        auto ids = std::vector<tbx::Uuid>();
        if (ids_array.is_array())
        {
            ids.reserve(ids_array.size());
            for (const auto& value : ids_array)
                if (value.is_number_unsigned())
                    ids.push_back(tbx::Uuid(value.get<uint64>()));
        }
        selection.ids = std::move(ids);

        for (const auto& id : selection.ids)
            if (auto world = world_of(id))
                world->get(id).add_tag(Tags::SELECTED, /*serialized*/ false);
    }
}
