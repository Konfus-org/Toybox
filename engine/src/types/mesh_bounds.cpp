#include "tbx/types/mesh_bounds.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/internal/mesh_bounds_internal.h"
#include "tbx/types/matrices.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace tbx
{
    bool try_compute_mesh_bounds(const Mesh& mesh, MeshBounds& out_bounds)
    {
        out_bounds = {};
        if (mesh.vertices.vertices.empty())
            return false;

        uint32 stride_bytes = 0U;
        uint32 position_offset_bytes = 0U;
        if (!internal::try_get_position_attribute(
                mesh.vertices.layout,
                stride_bytes,
                position_offset_bytes))
            return false;

        if (stride_bytes == 0U || (stride_bytes % static_cast<uint32>(sizeof(float))) != 0U
            || (position_offset_bytes % static_cast<uint32>(sizeof(float))) != 0U)
        {
            return false;
        }

        const uint32 stride_floats = stride_bytes / static_cast<uint32>(sizeof(float));
        const uint32 position_offset_floats =
            position_offset_bytes / static_cast<uint32>(sizeof(float));
        if (stride_floats == 0U || position_offset_floats + 2U >= stride_floats)
            return false;
        if ((mesh.vertices.vertices.size() % static_cast<size_t>(stride_floats)) != 0U)
            return false;

        const size_t vertex_count =
            mesh.vertices.vertices.size() / static_cast<size_t>(stride_floats);
        if (vertex_count == 0U)
            return false;

        Vec3 minimum = Vec3(std::numeric_limits<float>::max());
        Vec3 maximum = Vec3(std::numeric_limits<float>::lowest());

        for (size_t vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const size_t base_index = (vertex_index * static_cast<size_t>(stride_floats))
                                      + static_cast<size_t>(position_offset_floats);
            const Vec3 position = Vec3(
                mesh.vertices.vertices[base_index],
                mesh.vertices.vertices[base_index + 1U],
                mesh.vertices.vertices[base_index + 2U]);

            minimum = glm::min(minimum, position);
            maximum = glm::max(maximum, position);
        }

        const Vec3 center = (minimum + maximum) * 0.5F;
        float radius_squared = 0.0F;
        for (size_t vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const size_t base_index = (vertex_index * static_cast<size_t>(stride_floats))
                                      + static_cast<size_t>(position_offset_floats);
            const Vec3 position = Vec3(
                mesh.vertices.vertices[base_index],
                mesh.vertices.vertices[base_index + 1U],
                mesh.vertices.vertices[base_index + 2U]);

            const Vec3 offset = position - center;
            radius_squared = std::max(radius_squared, glm::dot(offset, offset));
        }

        out_bounds.minimum = minimum;
        out_bounds.maximum = maximum;
        out_bounds.sphere =
            Sphere {.center = center, .radius = std::sqrt(std::max(radius_squared, 0.0F))};
        out_bounds.is_valid = true;
        return true;
    }

    void update_mesh_bounds(Mesh& mesh)
    {
        auto bounds = MeshBounds {};
        if (try_compute_mesh_bounds(mesh, bounds))
        {
            mesh.bounds = bounds;
            return;
        }

        mesh.bounds = MeshBounds {};
    }

    Sphere transform_sphere(const Sphere& local_sphere, const Transform& transform)
    {
        const Mat4 model_to_world = build_transform_matrix(transform);
        const Vec4 transformed_center = model_to_world * Vec4(local_sphere.center, 1.0F);
        const float max_scale = internal::get_max_scale_component(transform.scale);
        return Sphere {
            .center = Vec3(transformed_center),
            .radius = local_sphere.radius * max_scale,
        };
    }
}
