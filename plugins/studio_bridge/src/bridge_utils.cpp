#include "bridge_utils.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/renderer.h"
#include "tbx/types/matrices.h"

namespace tbx::studio_bridge
{
    bool compute_world_focus(tbx::World& world, glm::vec3& out_focus)
    {
        auto sum = glm::vec3(0.0F);
        auto count = 0;
        for (auto& entity : world.get_with<tbx::Renderer>())
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
        if (!entity.has_component<tbx::Renderer>() || !entity.has_component<tbx::Transform>())
            return false;

        const auto model = assets.load<tbx::Model>(entity.get_component<tbx::Renderer>().model);
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
