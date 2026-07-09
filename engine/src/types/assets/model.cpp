#include "tbx/types/assets/model.h"
#include <glm/glm.hpp>
#include <limits>

namespace tbx
{
    Model::Model()
    {
        meshes = {Mesh::QUAD};
        parts = {ModelPart()};
        slots = {Handle()};
    }

    Model::Model(Mesh mesh)
    {
        meshes = {std::move(mesh)};
        parts = {ModelPart()};
        slots = {Handle()};
    }

    Model::Model(Mesh mesh, Handle slot)
    {
        meshes = {std::move(mesh)};
        parts = {ModelPart()};
        slots = {std::move(slot)};
    }

    bool ray_intersects_model(
        const Ray& world_ray, const Model& model, const Mat4& world_matrix, float& out_distance)
    {
        auto best = std::numeric_limits<float>::max();
        auto hit_any = false;
        for_each_model_mesh(
            model,
            world_matrix,
            [&world_ray, &best, &hit_any](const Mesh& mesh, const Mat4& mesh_matrix)
            {
                // Test in the mesh's local space; distances stay in world units because the local
                // ray's direction carries the matrix's scale (see the distance note on Ray).
                const auto local_ray = transform_ray(glm::inverse(mesh_matrix), world_ray);
                auto distance = 0.0F;
                if (ray_intersects_mesh(local_ray, mesh, distance) && distance > 0.0F
                    && distance < best)
                {
                    best = distance;
                    hit_any = true;
                }
            });

        if (hit_any)
            out_distance = best;
        return hit_any;
    }

    bool expand_aabb_with_model(
        const Model& model, const Mat4& world_matrix, Vec3& minimum, Vec3& maximum)
    {
        auto contributed = false;
        for_each_model_mesh(
            model,
            world_matrix,
            [&minimum, &maximum, &contributed](const Mesh& mesh, const Mat4& mesh_matrix)
            {
                if (!mesh.bounds.is_valid)
                    return;
                const auto lo = mesh.bounds.minimum;
                const auto hi = mesh.bounds.maximum;
                for (auto corner = 0; corner < 8; ++corner)
                {
                    const auto local = Vec3(
                        (corner & 1) ? hi.x : lo.x,
                        (corner & 2) ? hi.y : lo.y,
                        (corner & 4) ? hi.z : lo.z);
                    const auto point = Vec3(mesh_matrix * Vec4(local, 1.0F));
                    minimum = glm::min(minimum, point);
                    maximum = glm::max(maximum, point);
                }
                contributed = true;
            });
        return contributed;
    }
}
