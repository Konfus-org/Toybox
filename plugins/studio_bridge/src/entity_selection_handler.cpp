#include "entity_selection_handler.h"
#include "bridge_utils.h"
#include "wire.h"
#include "tbx/systems/graphics/screen_projection.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/collider.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/ray.h"
#include "tbx/types/raycast.h"
#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <memory>

namespace tbx::studio_bridge
{
    // Expands an evolving AABB to include a world-space point.
    static void expand_bounds(glm::vec3& minimum, glm::vec3& maximum, const glm::vec3& point)
    {
        minimum = glm::min(minimum, point);
        maximum = glm::max(maximum, point);
    }

    // Expands [minimum, maximum] by the world-space bounds of any physics collider/trigger shape on
    // the entity. Mirrors the physics shapes: box/sphere/capsule ignore scale (Jolt builds them from
    // the raw collider dimensions); mesh shapes are covered by the renderer bounds instead. Returns
    // whether it contributed. Lets invisible collider/trigger volumes (no render mesh) be picked.
    static bool accumulate_collider_world_bounds(
        tbx::Entity entity, glm::vec3& minimum, glm::vec3& maximum)
    {
        if (!entity.has_component<tbx::Transform>())
            return false;

        const auto world = entity.get_component<tbx::Transform>().to_world_space(entity);
        const auto center = glm::vec3(world.position);
        const auto rotation = world.rotation;

        const bool has_box = entity.has_component<tbx::BoxCollider>();
        if (has_box || entity.has_component<tbx::BoxTrigger>())
        {
            const auto half = has_box ? entity.get_component<tbx::BoxCollider>().half_extents
                                      : entity.get_component<tbx::BoxTrigger>().half_extents;
            for (auto i = 0; i < 8; ++i)
            {
                const auto local = glm::vec3(
                    (i & 1) ? half.x : -half.x,
                    (i & 2) ? half.y : -half.y,
                    (i & 4) ? half.z : -half.z);
                expand_bounds(minimum, maximum, center + (rotation * local));
            }
            return true;
        }

        const bool has_sphere = entity.has_component<tbx::SphereCollider>();
        if (has_sphere || entity.has_component<tbx::SphereTrigger>())
        {
            const auto radius = has_sphere ? entity.get_component<tbx::SphereCollider>().radius
                                           : entity.get_component<tbx::SphereTrigger>().radius;
            expand_bounds(minimum, maximum, center - glm::vec3(radius));
            expand_bounds(minimum, maximum, center + glm::vec3(radius));
            return true;
        }

        const bool has_capsule = entity.has_component<tbx::CapsuleCollider>();
        if (has_capsule || entity.has_component<tbx::CapsuleTrigger>())
        {
            const auto radius = has_capsule ? entity.get_component<tbx::CapsuleCollider>().radius
                                            : entity.get_component<tbx::CapsuleTrigger>().radius;
            const auto half_height =
                has_capsule ? entity.get_component<tbx::CapsuleCollider>().half_height
                            : entity.get_component<tbx::CapsuleTrigger>().half_height;
            const auto up = glm::vec3(rotation * glm::vec3(0.0F, 1.0F, 0.0F));
            for (const auto& cap : {center + (up * half_height), center - (up * half_height)})
            {
                expand_bounds(minimum, maximum, cap - glm::vec3(radius));
                expand_bounds(minimum, maximum, cap + glm::vec3(radius));
            }
            return true;
        }

        return false;
    }

    // Slab test: the parametric distance along the ray at which it first enters the AABB. False when
    // the ray misses (or the box is entirely behind it). A ray starting inside the box returns 0.
    static bool ray_aabb(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& minimum,
        const glm::vec3& maximum,
        float& out_distance)
    {
        auto t_near = 0.0F;
        auto t_far = std::numeric_limits<float>::max();
        for (auto axis = 0; axis < 3; ++axis)
        {
            if (std::abs(direction[axis]) < 1e-8F)
            {
                if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis])
                    return false; // parallel to the slab and outside it
                continue;
            }
            const auto inverse = 1.0F / direction[axis];
            auto t0 = (minimum[axis] - origin[axis]) * inverse;
            auto t1 = (maximum[axis] - origin[axis]) * inverse;
            if (t0 > t1)
                std::swap(t0, t1);
            t_near = std::max(t_near, t0);
            t_far = std::min(t_far, t1);
            if (t_near > t_far)
                return false;
        }
        out_distance = t_near;
        return true;
    }

    // Loads a renderer's model, skipping (and remembering) handles that have already failed so a dangling
    // handle isn't reloaded — and re-warned about — on every pick/occlusion call.
    static std::shared_ptr<tbx::Model> try_load_model(
        tbx::AssetManager& assets,
        const tbx::Handle& handle,
        std::unordered_set<std::uint64_t>& unloadable)
    {
        const auto key = static_cast<std::uint64_t>(handle.id.value);
        if (unloadable.contains(key))
            return {};
        auto model = assets.load<tbx::Model>(handle);
        if (!model)
        {
            unloadable.insert(key);
            return {};
        }
        return model;
    }

    // Nearest triangle-precise hit of a world ray against a model (parts carry their own transform,
    // else each mesh draws at the model root — mirrors the renderer and the old picker).
    static bool ray_hits_model(
        const tbx::Ray& world_ray,
        const tbx::Model& model,
        const glm::mat4& world_matrix,
        float& out_distance)
    {
        auto best = std::numeric_limits<float>::max();
        auto hit_any = false;
        const auto test = [&](const tbx::Mesh& mesh, const glm::mat4& mesh_matrix)
        {
            const auto local_ray = tbx::transform_ray(glm::inverse(mesh_matrix), world_ray);
            auto distance = 0.0F;
            if (tbx::ray_intersects_mesh(local_ray, mesh, distance) && distance > 0.0F
                && distance < best)
            {
                best = distance;
                hit_any = true;
            }
        };

        if (!model.parts.empty())
        {
            for (const auto& part : model.parts)
                if (part.mesh_index < model.meshes.size())
                    test(model.meshes[part.mesh_index], world_matrix * part.transform);
        }
        else
        {
            for (const auto& mesh : model.meshes)
                test(mesh, world_matrix);
        }

        if (hit_any)
            out_distance = best;
        return hit_any;
    }

    // World-space AABB of a model's mesh bounds (broad-phase cull for occlusion). False when no bounds.
    static bool model_world_aabb(
        const tbx::Model& model,
        const glm::mat4& world_matrix,
        glm::vec3& out_min,
        glm::vec3& out_max)
    {
        auto contributed = false;
        const auto expand = [&](const tbx::Mesh& mesh, const glm::mat4& mesh_matrix)
        {
            if (!mesh.bounds.is_valid)
                return;
            const auto lo = mesh.bounds.minimum;
            const auto hi = mesh.bounds.maximum;
            for (auto corner = 0; corner < 8; ++corner)
            {
                const auto local = glm::vec3(
                    (corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z);
                const auto point = glm::vec3(mesh_matrix * glm::vec4(local, 1.0F));
                out_min = glm::min(out_min, point);
                out_max = glm::max(out_max, point);
            }
            contributed = true;
        };

        if (!model.parts.empty())
        {
            for (const auto& part : model.parts)
                if (part.mesh_index < model.meshes.size())
                    expand(model.meshes[part.mesh_index], world_matrix * part.transform);
        }
        else
        {
            for (const auto& mesh : model.meshes)
                expand(mesh, world_matrix);
        }
        return contributed;
    }

    EntitySelectionHandler::EntitySelectionHandler(EngineServices& services, ViewManager& views)
        : _services(services)
        , _views(views)
    {
    }

    Result EntitySelectionHandler::pick(const tbx::Json& params, tbx::Json& out_reply)
    {
        const auto u = params.value("u", 0.0F);
        const auto v = params.value("v", 0.0F);

        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.get().resolve_view_camera(params.value(Wire::VIEW, std::string()), camera_view);
            !resolved)
            return resolved;

        // Pick against the view's own world: an asset-preview view's isolated preview world, otherwise
        // the active world.
        auto world = _views.get().resolve_view_world(params.value(Wire::VIEW, std::string()));
        auto assets = _services.get().asset_manager.lock();
        if (!world || !assets)
            return Result(false, "No world to pick in.");

        // Unproject the normalized click into a world-space ray (correct for both perspective and
        // orthographic cameras); the same CameraView helper the gizmo uses, so the two stay in lock-step.
        const auto world_ray = camera_view.cursor_ray(u, v);
        auto best_distance = std::numeric_limits<float>::max();
        auto hit = tbx::Entity();

        // Pass 1: triangle-precise against renderable meshes — the geometry you actually see and click.
        // (An AABB-only test wrongly picks any large entity whose bounds enclose the camera, e.g. a room
        // interior, because the ray starts inside it.)
        for (auto entity : world->get_with<tbx::Renderer, tbx::Transform>())
        {
            const auto model =
                try_load_model(*assets, entity.get_component<tbx::Renderer>().model, _unloadable_models);
            if (!model || model->meshes.empty())
                continue;

            const auto world_matrix = tbx::build_transform_matrix(
                entity.get_component<tbx::Transform>().to_world_space(entity));
            auto distance = 0.0F;
            if (ray_hits_model(world_ray, *model, world_matrix, distance) && distance < best_distance)
            {
                best_distance = distance;
                hit = entity;
            }
        }

        // Pass 2: invisible collider/trigger volumes (no renderable mesh) via their shape bounds, so
        // sensors/lights can still be clicked. Front-face only (distance > epsilon) so a volume that
        // merely encloses the camera doesn't swallow every click.
        for (auto entity : world->get_with<tbx::Transform>())
        {
            if (entity.has_component<tbx::Renderer>())
                continue;

            auto minimum = glm::vec3(std::numeric_limits<float>::max());
            auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
            if (!accumulate_collider_world_bounds(entity, minimum, maximum))
                continue;

            auto distance = 0.0F;
            if (ray_aabb(world_ray.origin, world_ray.direction, minimum, maximum, distance)
                && distance > 1e-3F && distance < best_distance)
            {
                best_distance = distance;
                hit = entity;
            }
        }

        // Pass 3: physics fallback for solid bodies whose authoring geometry we can't resolve (only
        // hits while playing, when bodies exist).
        if (!hit.get_id().is_valid())
        {
            if (const auto physics = _services.get().physics.lock())
            {
                const auto query = tbx::RaycastQuery {
                    .ray = world_ray,
                    .max_distance = 100000.0F,
                };
                if (const auto result = physics->raycast(query))
                    hit = world->get(result.hit_entity_id);
            }
        }

        if (hit.get_id().is_valid())
            out_reply[Wire::ID] = hit.get_id().value;
        else
            out_reply[Wire::ID] = nullptr;
        return Result::OK;
    }

    Result EntitySelectionHandler::pick_rect(const tbx::Json& params, tbx::Json& out_reply)
    {
        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.get().resolve_view_camera(params.value(Wire::VIEW, std::string()), camera_view);
            !resolved)
            return resolved;

        auto world = _views.get().resolve_view_world(params.value(Wire::VIEW, std::string()));
        auto assets = _services.get().asset_manager.lock();
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

        // Window select: an entity is picked when the centre of its world bounds projects inside the
        // marquee (and is in front of the camera). Centre-inside is predictable and avoids the
        // partial off-screen edge cases a full-bounds test would introduce. Bounds come from the
        // renderer and/or a collider/trigger shape, so invisible physics volumes marquee-select too.
        auto ids = tbx::Json::array();
        for (auto entity : world->get_with<tbx::Transform>())
        {
            auto minimum = glm::vec3(std::numeric_limits<float>::max());
            auto maximum = glm::vec3(std::numeric_limits<float>::lowest());
            const bool has_render_bounds =
                accumulate_entity_world_bounds(*assets, entity, minimum, maximum);
            const bool has_collider_bounds =
                accumulate_collider_world_bounds(entity, minimum, maximum);
            if (!has_render_bounds && !has_collider_bounds)
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

        out_reply[Wire::IDS] = std::move(ids);
        return Result::OK;
    }

    Result EntitySelectionHandler::project_entities(const tbx::Json& params, tbx::Json& out_reply)
    {
        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.get().resolve_view_camera(params.value(Wire::VIEW, std::string()), camera_view);
            !resolved)
            return resolved;

        // Project against the view's own world (an asset-preview view's isolated world, otherwise the
        // active world). The engine projector owns the world→screen maths; the editor polls this and
        // draws/filters the overlay, so the bridge just answers the request.
        auto world = _views.get().resolve_view_world(params.value(Wire::VIEW, std::string()));
        if (!world)
            return Result(false, "No world to project.");

        auto items = tbx::Json::array();
        for (const auto& position : tbx::project_entities_to_screen(camera_view, *world))
        {
            auto entry = tbx::Json::object();
            entry[Wire::ID] = position.id.value;
            entry["u"] = position.u;
            entry["v"] = position.v;
            entry["depth"] = position.depth;
            items.push_back(std::move(entry));
        }

        out_reply["items"] = std::move(items);
        return Result::OK;
    }

    Result EntitySelectionHandler::query_occlusion(const tbx::Json& params, tbx::Json& out_reply)
    {
        auto camera_view = tbx::CameraView();
        if (const auto resolved =
                _views.get().resolve_view_camera(params.value(Wire::VIEW, std::string()), camera_view);
            !resolved)
            return resolved;

        auto world = _views.get().resolve_view_world(params.value(Wire::VIEW, std::string()));
        auto assets = _services.get().asset_manager.lock();
        if (!world || !assets)
            return Result(false, "No world to test occlusion in.");

        auto occluded = tbx::Json::array();
        const auto ids_iterator = params.find(Wire::IDS);
        if (ids_iterator == params.end() || !ids_iterator->is_array())
        {
            out_reply[Wire::OCCLUDED] = std::move(occluded);
            return Result::OK;
        }

        // Gather the scene's renderable meshes once (each model loaded a single time this query), so
        // refreshing every billboard icon costs one pass over the scene rather than one per icon.
        struct Occluder
        {
            tbx::Uuid id = {};
            std::shared_ptr<tbx::Model> model = {};
            glm::mat4 matrix = glm::mat4(1.0F);
            glm::vec3 minimum = glm::vec3(0.0F);
            glm::vec3 maximum = glm::vec3(0.0F);
            bool has_aabb = false;
        };
        auto occluders = std::vector<Occluder>();
        for (auto entity : world->get_with<tbx::Renderer, tbx::Transform>())
        {
            auto model =
                try_load_model(*assets, entity.get_component<tbx::Renderer>().model, _unloadable_models);
            if (!model || model->meshes.empty())
                continue;

            auto occluder = Occluder {
                .id = entity.get_id(),
                .model = std::move(model),
                .matrix = tbx::build_transform_matrix(
                    entity.get_component<tbx::Transform>().to_world_space(entity)),
                .minimum = glm::vec3(std::numeric_limits<float>::max()),
                .maximum = glm::vec3(std::numeric_limits<float>::lowest()),
            };
            occluder.has_aabb =
                model_world_aabb(*occluder.model, occluder.matrix, occluder.minimum, occluder.maximum);
            occluders.push_back(std::move(occluder));
        }

        const auto camera = glm::vec3(camera_view.position);
        for (const auto& id_value : *ids_iterator)
        {
            auto is_occluded = false;
            if (id_value.is_number_unsigned())
            {
                const auto id = tbx::Uuid(id_value.get<uint32>());
                auto entity = world->get(id);
                if (entity.get_id().is_valid() && entity.has_component<tbx::Transform>())
                {
                    const auto target = glm::vec3(
                        entity.get_component<tbx::Transform>().to_world_space(entity).position);
                    const auto delta = target - camera;
                    const auto distance = glm::length(delta);
                    if (distance > 1e-4F)
                    {
                        const auto direction = delta / distance;
                        const auto ray = tbx::Ray {.origin = camera, .direction = direction};
                        for (const auto& occluder : occluders)
                        {
                            if (occluder.id == id)
                                continue;
                            if (occluder.has_aabb)
                            {
                                auto t_aabb = 0.0F;
                                if (!ray_aabb(camera, direction, occluder.minimum, occluder.maximum, t_aabb)
                                    || t_aabb >= distance)
                                    continue;
                            }
                            auto hit = 0.0F;
                            if (ray_hits_model(ray, *occluder.model, occluder.matrix, hit)
                                && hit < distance - 1e-3F)
                            {
                                is_occluded = true;
                                break;
                            }
                        }
                    }
                }
            }
            occluded.push_back(is_occluded);
        }

        out_reply[Wire::OCCLUDED] = std::move(occluded);
        return Result::OK;
    }
}
