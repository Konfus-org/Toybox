#include "bridge_geometry.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/matrices.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

namespace tbx::studio_bridge
{
    tbx::Quat look_rotation(const glm::vec3& forward, const glm::vec3& world_up)
    {
        const auto f = glm::normalize(forward);
        // Near-vertical looks have no stable horizon against world up; fall back to a different
        // axis.
        auto up_reference = world_up;
        if (std::abs(glm::dot(f, up_reference)) > 0.999F)
            up_reference = glm::vec3(0.0F, 0.0F, 1.0F);

        const auto right = glm::normalize(glm::cross(f, up_reference));
        const auto up = glm::cross(right, f);
        return glm::normalize(glm::quat_cast(glm::mat3(right, up, -f)));
    }

    bool compute_world_focus(tbx::World& world, glm::vec3& out_focus)
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (auto& entity : world.get_with<tbx::StaticMesh>())
        {
            if (!entity.has_component<tbx::Transform>())
                continue;

            sum += entity.get_component<tbx::Transform>().to_world_space(entity).position;
            ++count;
        }

        if (count == 0)
            return false;

        out_focus = sum / static_cast<float>(count);
        return true;
    }

    bool accumulate_entity_world_bounds(
        tbx::AssetManager& assets,
        tbx::Entity entity,
        glm::vec3& out_min,
        glm::vec3& out_max)
    {
        if (!entity.has_component<tbx::StaticMesh>() || !entity.has_component<tbx::Transform>())
            return false;

        const auto model = assets.load<tbx::Model>(entity.get_component<tbx::StaticMesh>().handle);
        if (!model || model->meshes.empty())
            return false;

        const auto world_matrix = tbx::build_transform_matrix(
            entity.get_component<tbx::Transform>().to_world_space(entity));

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
                    (corner & 1) ? hi.x : lo.x,
                    (corner & 2) ? hi.y : lo.y,
                    (corner & 4) ? hi.z : lo.z);
                const auto point = glm::vec3(mesh_matrix * glm::vec4(local, 1.0F));
                out_min = glm::min(out_min, point);
                out_max = glm::max(out_max, point);
            }
            contributed = true;
        };

        if (!model->parts.empty())
        {
            for (const auto& part : model->parts)
                if (part.mesh_index < model->meshes.size())
                    expand(model->meshes[part.mesh_index], world_matrix * part.transform);
        }
        else
        {
            for (const auto& mesh : model->meshes)
                expand(mesh, world_matrix);
        }

        return contributed;
    }

    bool is_self_or_descendant(tbx::World& world, tbx::Entity entity, const tbx::Uuid& ancestor)
    {
        auto current = entity;
        // The depth guard is a cheap safeguard against a malformed (cyclic) parent chain.
        for (auto guard = 0; current.get_id().is_valid() && guard < 4096; ++guard)
        {
            if (current.get_id().value == ancestor.value)
                return true;

            const auto parent_id = current.get_parent();
            if (!parent_id.is_valid())
                break;
            current = world.get(parent_id);
        }

        return false;
    }

    void make_cursor_ray(
        const tbx::CameraView& camera_view,
        float u,
        float v,
        glm::vec3& out_origin,
        glm::vec3& out_direction)
    {
        const auto vp = camera_view.camera.get_view_projection_matrix(
            camera_view.position,
            camera_view.rotation);
        const auto inverse_vp = glm::inverse(vp);
        const auto ndc_x = (u * 2.0F) - 1.0F;
        const auto ndc_y = 1.0F - (v * 2.0F);
        const auto unproject = [&](float ndc_z)
        {
            const auto point = inverse_vp * glm::vec4(ndc_x, ndc_y, ndc_z, 1.0F);
            return glm::vec3(point) / point.w;
        };
        const auto near_point = unproject(-1.0F);
        out_origin = near_point;
        out_direction = glm::normalize(unproject(1.0F) - near_point);
    }

    float closest_param_on_axis(
        const glm::vec3& ray_origin,
        const glm::vec3& ray_direction,
        const glm::vec3& pivot,
        const glm::vec3& axis)
    {
        const auto r = ray_origin - pivot;
        const auto b = glm::dot(axis, ray_direction);
        const auto d = glm::dot(axis, r);
        const auto e = glm::dot(ray_direction, r);
        const auto denom = 1.0F - (b * b);
        if (std::abs(denom) < 1e-5F)
            return 0.0F; // ray (almost) parallel to the axis
        // Closest point between the cursor ray and the axis line through pivot. The parameter grows
        // in the +axis direction as the cursor moves that way on screen (so a handle follows the
        // cursor rather than running opposite to it).
        return (d - (b * e)) / denom;
    }

    bool project_to_screen(
        const glm::mat4& view_projection,
        const glm::vec3& world,
        float& out_u,
        float& out_v)
    {
        const auto clip = view_projection * glm::vec4(world, 1.0F);
        if (clip.w <= 0.0F)
            return false;

        const auto ndc = glm::vec3(clip) / clip.w;
        out_u = (ndc.x * 0.5F) + 0.5F;
        out_v = 0.5F - (ndc.y * 0.5F);
        return true;
    }

    float distance_point_segment(float px, float py, float ax, float ay, float bx, float by)
    {
        const auto dx = bx - ax;
        const auto dy = by - ay;
        const auto length_sq = (dx * dx) + (dy * dy);
        auto t = length_sq > 1e-12F ? (((px - ax) * dx) + ((py - ay) * dy)) / length_sq : 0.0F;
        t = std::clamp(t, 0.0F, 1.0F);
        const auto cx = ax + (t * dx);
        const auto cy = ay + (t * dy);
        return std::sqrt(((px - cx) * (px - cx)) + ((py - cy) * (py - cy)));
    }

    bool ray_plane(
        const glm::vec3& origin,
        const glm::vec3& direction,
        const glm::vec3& p0,
        const glm::vec3& normal,
        glm::vec3& out_hit)
    {
        const auto denom = glm::dot(direction, normal);
        if (std::abs(denom) < 1e-6F)
            return false;
        const auto t = glm::dot(p0 - origin, normal) / denom;
        out_hit = origin + (t * direction);
        return true;
    }

    float signed_angle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& axis)
    {
        if (glm::length(a) < 1e-6F || glm::length(b) < 1e-6F)
            return 0.0F;
        const auto x = glm::dot(a, b);
        const auto y = glm::dot(glm::cross(a, b), axis);
        return std::atan2(y, x);
    }

    tbx::Transform world_to_local_for(tbx::Entity entity, const tbx::Transform& new_world)
    {
        auto parent = tbx::Entity();
        if (entity.try_get_parent_entity(parent) && parent.has_component<tbx::Transform>())
            return tbx::world_to_local_tranform(
                parent.get_component<tbx::Transform>().to_world_space(parent),
                new_world);
        return new_world;
    }
}
