#include "hover_ops.h"
#include "engine_services.h"
#include "hover_state.h"
#include "picking_ops.h"
#include "tags.h"
#include "view_ops.h"
#include "view_state.h"
#include "view_stream.h"
#include "wire.h"
#include "tbx/systems/files/json.h"
#include <string>
#include <utility>

namespace tbx::studio_bridge
{
    // Moves the hover to `id` (invalid = none): swaps the editor.hovered runtime tag from the old
    // entity to the new one and notifies the editor. A no-op when the hover is already `id`.
    static void apply_hover(
        HoverState& hover,
        const EngineServices& services,
        const std::string& view_name,
        const std::shared_ptr<tbx::World>& world,
        const tbx::Uuid& id)
    {
        if (id.value == hover.hovered.value)
            return;

        // Un-tag the previous entity through the world it was tagged in (which may have been
        // swapped or closed since — the weak_ptr guards that, and a dead world dropped the tag
        // with the entity anyway).
        if (hover.hovered.is_valid())
            if (const auto old_world = hover.hovered_world.lock())
            {
                auto old_entity = old_world->get(hover.hovered);
                if (old_entity.get_id().is_valid())
                    old_entity.remove_tag(Tags::HOVERED);
            }

        if (id.is_valid() && world)
            world->get(id).add_tag(Tags::HOVERED, /*serialized*/ false);

        hover.hovered = id;
        hover.hovered_world = world;
        hover.view = view_name;

        if (const auto host = services.rpc_host.lock())
        {
            auto params = tbx::Json::object();
            params[Wire::VIEW] = view_name;
            if (id.is_valid())
                params[Wire::ID] = id.value;
            else
                params[Wire::ID] = nullptr;
            host->send_notification(Wire::VIEW_HOVER, params);
        }
    }

    void update_hover(
        HoverState& hover,
        PickingState& picking,
        const EngineServices& services,
        ViewState& views)
    {
        // Snapshot the focused editor view's camera + cursor under the views lock; the raycast runs
        // outside it (it touches only the world + asset manager).
        auto camera_view = tbx::CameraView();
        auto view_name = std::string();
        auto world_id = 0U;
        auto buttons = 0U;
        auto u = 0.0F;
        auto v = 0.0F;
        auto found = false;
        with_views_locked(
            views,
            [&](std::vector<std::unique_ptr<ViewStream>>& view_streams,
                std::unordered_map<std::string, ViewInput>& inputs)
            {
                for (auto& view_ptr : view_streams)
                {
                    auto* view = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (view == nullptr || !view->view.is_valid)
                        continue;
                    const auto& input = inputs[view->name];
                    if (!input.focused)
                        continue;
                    camera_view = view->view;
                    view_name = view->name;
                    world_id = view->world_id;
                    buttons = input.buttons;
                    u = input.cursor_u;
                    v = input.cursor_v;
                    found = true;
                    return;
                }
            });

        if (!found)
        {
            // No focused editor view: clear any lingering hover and forget the raycast snapshot so
            // refocusing re-raycasts immediately.
            apply_hover(hover, services, hover.view, nullptr, tbx::Uuid());
            hover.has_last = false;
            return;
        }

        // Mid-drag (gizmo or fly camera) the hover freezes; it refreshes on release via the
        // cursor/camera-moved check below (a drag always moves one of them).
        if (buttons != 0U)
            return;

        const auto moved = !hover.has_last
            || u != hover.last_u
            || v != hover.last_v
            || camera_view.position != hover.last_camera_position
            || camera_view.rotation != hover.last_camera_rotation;
        if (!moved)
            return;
        hover.last_u = u;
        hover.last_v = v;
        hover.last_camera_position = camera_view.position;
        hover.last_camera_rotation = camera_view.rotation;
        hover.has_last = true;

        auto world =
            world_id == 0U ? services.active_world() : resolve_world_by_id(views, world_id);
        if (!world)
        {
            apply_hover(hover, services, view_name, nullptr, tbx::Uuid());
            return;
        }

        const auto hit = raycast_entity(picking, services, camera_view, *world, u, v);
        apply_hover(hover, services, view_name, world, hit.get_id());
    }
}
