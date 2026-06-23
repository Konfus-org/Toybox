#include "tbx/types/ray.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <limits>

namespace tbx
{
    Ray transform_ray(const Mat4& matrix, const Ray& ray)
    {
        return Ray {
            .origin = Vec3(matrix * Vec4(ray.origin, 1.0F)),
            .direction = Vec3(matrix * Vec4(ray.direction, 0.0F)),
        };
    }

    bool ray_intersects_aabb(
        const Ray& ray,
        const Vec3& minimum,
        const Vec3& maximum,
        float& out_distance)
    {
        auto t_near = std::numeric_limits<float>::lowest();
        auto t_far = std::numeric_limits<float>::max();
        for (auto axis = 0; axis < 3; ++axis)
        {
            const auto origin = ray.origin[axis];
            const auto direction = ray.direction[axis];
            if (std::abs(direction) < 1e-8F)
            {
                // Parallel to this slab: a miss unless the origin already lies between its planes.
                if (origin < minimum[axis] || origin > maximum[axis])
                    return false;
                continue;
            }

            auto t0 = (minimum[axis] - origin) / direction;
            auto t1 = (maximum[axis] - origin) / direction;
            if (t0 > t1)
                std::swap(t0, t1);
            t_near = std::max(t_near, t0);
            t_far = std::min(t_far, t1);
            if (t_near > t_far)
                return false;
        }

        if (t_near <= 0.0F)
            return false; // origin is inside (or the box is behind); reject

        out_distance = t_near;
        return true;
    }

    bool ray_intersects_triangle(
        const Ray& ray,
        const Vec3& a,
        const Vec3& b,
        const Vec3& c,
        float& out_distance)
    {
        constexpr auto EPSILON = 1e-7F;
        const auto edge1 = b - a;
        const auto edge2 = c - a;
        const auto p = glm::cross(ray.direction, edge2);
        const auto determinant = glm::dot(edge1, p);
        if (std::abs(determinant) < EPSILON)
            return false; // ray parallel to the triangle

        const auto inverse = 1.0F / determinant;
        const auto to_origin = ray.origin - a;
        const auto u = glm::dot(to_origin, p) * inverse;
        if (u < 0.0F || u > 1.0F)
            return false;

        const auto q = glm::cross(to_origin, edge1);
        const auto v = glm::dot(ray.direction, q) * inverse;
        if (v < 0.0F || (u + v) > 1.0F)
            return false;

        out_distance = glm::dot(edge2, q) * inverse;
        return out_distance > EPSILON; // in front of the origin
    }

    bool ray_intersects_mesh(const Ray& local_ray, const Mesh& mesh, float& out_distance)
    {
        // Broad phase: skip the whole mesh when the ray misses its bounds (the "entry in front"
        // rule also rejects a box enclosing the origin, e.g. a skybox).
        if (mesh.bounds.is_valid)
        {
            auto bounds_distance = 0.0F;
            if (!ray_intersects_aabb(
                    local_ray,
                    mesh.bounds.minimum,
                    mesh.bounds.maximum,
                    bounds_distance))
                return false;
        }

        const auto position = [&mesh](uint32 index)
        {
            return Vec3(read_vertex_buffer_attribute(
                mesh.vertices,
                index,
                vertex_attribute_position_debug_name,
                Vec4(0.0F)));
        };

        auto best = std::numeric_limits<float>::max();
        auto hit = false;
        const auto test = [&](uint32 i0, uint32 i1, uint32 i2)
        {
            auto distance = 0.0F;
            if (ray_intersects_triangle(
                    local_ray,
                    position(i0),
                    position(i1),
                    position(i2),
                    distance)
                && distance < best)
            {
                best = distance;
                hit = true;
            }
        };

        if (!mesh.indices.empty())
        {
            for (size i = 0U; i + 2U < mesh.indices.size(); i += 3U)
                test(mesh.indices[i], mesh.indices[i + 1U], mesh.indices[i + 2U]);
        }
        else
        {
            // No index buffer: treat the vertices as a sequential triangle list.
            const auto stride = mesh.vertices.layout.stride / static_cast<uint32>(sizeof(float));
            const auto vertex_count =
                stride > 0U ? static_cast<uint32>(mesh.vertices.vertices.size() / stride) : 0U;
            for (uint32 i = 0U; i + 2U < vertex_count; i += 3U)
                test(i, i + 1U, i + 2U);
        }

        if (hit)
            out_distance = best;
        return hit;
    }
}
