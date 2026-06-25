#include "pick_service.h"
#include "bridge_geometry.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/ray.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <limits>

namespace tbx::studio_bridge
{
    PickService::PickService(EngineServices& services, ViewManager& views)
        : _services(services)
        , _views(views)
    {
    }

    Result PickService::pick(const tbx::Json& params, tbx::Json& out_reply)
    {
        const auto u = params.value("u", 0.0F);
        const auto v = params.value("v", 0.0F);

        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.resolve_view_camera(params.value("view", std::string()), camera_view);
            !resolved)
            return resolved;

        // Pick against the view's own world: an asset-preview view's isolated preview world, otherwise
        // the active world.
        auto world = _views.resolve_view_world(params.value("view", std::string()));
        auto assets = _services.asset_manager.lock();
        if (!world || !assets)
            return Result(false, "No world to pick in.");

        // Unproject the normalized click into a world-space ray (correct for both perspective and
        // orthographic cameras); shared with the gizmo picker so the two stay in lock-step.
        auto ray_origin = glm::vec3(0.0F);
        auto ray_direction = glm::vec3(0.0F, 0.0F, -1.0F);
        make_cursor_ray(camera_view, u, v, ray_origin, ray_direction);
        const auto world_ray = tbx::Ray {.origin = ray_origin, .direction = ray_direction};

        // Nearest static mesh whose actual geometry the ray hits (triangle-precise, not just the
        // bounding box), via the engine's intersection lib. Tested in each mesh's local space so
        // rotation/scale are exact.
        auto best_distance = std::numeric_limits<float>::max();
        auto hit = tbx::Entity();
        for (auto entity : world->get_with<tbx::Renderer, tbx::Transform>())
        {
            const auto model =
                assets->load<tbx::Model>(entity.get_component<tbx::Renderer>().model);
            if (!model || model->meshes.empty())
                continue;

            const auto world_matrix = tbx::build_transform_matrix(
                entity.get_component<tbx::Transform>().to_world_space(entity));

            const auto test_mesh = [&](const tbx::Mesh& mesh, const glm::mat4& mesh_matrix)
            {
                const auto local_ray = tbx::transform_ray(glm::inverse(mesh_matrix), world_ray);
                auto distance = 0.0F;
                if (tbx::ray_intersects_mesh(local_ray, mesh, distance) && distance < best_distance)
                {
                    best_distance = distance;
                    hit = entity;
                }
            };

            // Mirror the renderer: parts carry their own transform; otherwise each mesh draws at
            // the model root.
            if (!model->parts.empty())
            {
                for (const auto& part : model->parts)
                    if (part.mesh_index < model->meshes.size())
                        test_mesh(model->meshes[part.mesh_index], world_matrix * part.transform);
            }
            else
            {
                for (const auto& mesh : model->meshes)
                    test_mesh(mesh, world_matrix);
            }
        }

        if (hit.get_id().is_valid())
            out_reply["id"] = hit.get_id().value;
        else
            out_reply["id"] = nullptr;
        return Result::OK;
    }

    Result PickService::pick_rect(const tbx::Json& params, tbx::Json& out_reply)
    {
        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.resolve_view_camera(params.value("view", std::string()), camera_view);
            !resolved)
            return resolved;

        auto world = _views.resolve_view_world(params.value("view", std::string()));
        auto assets = _services.asset_manager.lock();
        if (!world || !assets)
            return Result(false, "No world to pick in.");

        // Normalized marquee rect, top-left origin (the editor's coordinate convention).
        const auto min_u = std::min(params.value("u0", 0.0F), params.value("u1", 0.0F));
        const auto max_u = std::max(params.value("u0", 0.0F), params.value("u1", 0.0F));
        const auto min_v = std::min(params.value("v0", 0.0F), params.value("v1", 0.0F));
        const auto max_v = std::max(params.value("v0", 0.0F), params.value("v1", 0.0F));

        const auto view_projection = camera_view.camera.get_view_projection_matrix(
            camera_view.position,
            camera_view.rotation);

        // Window select: a static mesh is picked when the centre of its world bounds projects
        // inside the marquee (and is in front of the camera). Centre-inside is predictable and
        // avoids the partial off-screen edge cases a full-bounds test would introduce.
        auto ids = tbx::Json::array();
        for (auto entity : world->get_with<tbx::Renderer, tbx::Transform>())
        {
            auto minimum = glm::vec3(std::numeric_limits<float>::max());
            auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
            if (!accumulate_entity_world_bounds(*assets, entity, minimum, maximum))
                continue;

            const auto center = (minimum + maximum) * 0.5F;
            const auto clip = view_projection * glm::vec4(center, 1.0F);
            if (clip.w <= 0.0F)
                continue; // behind the camera

            const auto ndc = glm::vec3(clip) / clip.w;
            const auto screen_u = (ndc.x * 0.5F) + 0.5F;
            const auto screen_v = 0.5F - (ndc.y * 0.5F); // flip to top-left origin
            if (screen_u >= min_u && screen_u <= max_u && screen_v >= min_v && screen_v <= max_v)
                ids.push_back(entity.get_id().value);
        }

        out_reply["ids"] = std::move(ids);
        return Result::OK;
    }
}
