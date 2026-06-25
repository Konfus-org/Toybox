#include "billboard_publisher.h"
#include "bridge_geometry.h"
#include "tbx/systems/files/json.h"
#include "tbx/systems/graphics/camera_view.h"
#include "tbx/types/components/transform.h"
#include <glm/glm.hpp>
#include <vector>

namespace tbx::studio_bridge
{
    BillboardPublisher::BillboardPublisher(EngineServices& services, ViewManager& views)
        : _services(services)
        , _views(views)
    {
    }

    void BillboardPublisher::publish()
    {
        const auto host = _services.rpc_host.lock();
        if (!host || !host->has_client())
            return;

        auto world = _services.active_world();
        if (!world)
            return;

        // World positions are view-independent, so snapshot them once and reproject per editor view.
        struct Item
        {
            uint64 id = 0U;
            glm::vec3 position = glm::vec3(0.0F);
        };
        auto items = std::vector<Item>();
        for (auto entity : world->get_with<tbx::Transform>())
            items.push_back(
                Item {
                    .id = entity.get_id().value,
                    .position = glm::vec3(
                        entity.get_component<tbx::Transform>().to_world_space(entity).position),
                });

        // Gather each editor view's camera under the views lock, then project + send outside it so the
        // (potentially slow) notification never blocks the view collection.
        struct ViewCamera
        {
            std::string name = {};
            glm::mat4 view_projection = glm::mat4(1.0F);
            glm::vec3 position = glm::vec3(0.0F);
        };
        auto cameras = std::vector<ViewCamera>();
        _views.with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views, tbx::EntityRegistry& registry)
            {
                for (auto& view : views)
                {
                    if (view->kind != ViewKind::Editor || !view->camera_id.is_valid())
                        continue;
                    auto camera_entity = registry.get(view->camera_id);
                    if (!camera_entity.get_id().is_valid())
                        continue;
                    const auto camera_view = tbx::CameraView::from_entity(camera_entity);
                    if (!camera_view.is_valid)
                        continue;
                    cameras.push_back(
                        ViewCamera {
                            .name = view->name,
                            .view_projection = camera_view.camera.get_view_projection_matrix(
                                camera_view.position, camera_view.rotation),
                            .position = glm::vec3(camera_view.position),
                        });
                }
            });

        for (const auto& camera : cameras)
        {
            auto entries = tbx::Json::array();
            for (const auto& item : items)
            {
                auto u = 0.0F;
                auto v = 0.0F;
                if (!project_to_screen(camera.view_projection, item.position, u, v))
                    continue; // behind the camera

                auto entry = tbx::Json::object();
                entry["id"] = item.id;
                entry["u"] = u;
                entry["v"] = v;
                entry["depth"] = glm::length(item.position - camera.position);
                entries.push_back(std::move(entry));
            }

            auto params = tbx::Json::object();
            params["view"] = camera.name;
            params["items"] = std::move(entries);
            host->send_notification("view.billboards", params);
        }
    }
}
