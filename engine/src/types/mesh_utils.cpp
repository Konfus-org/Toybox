#include "tbx/types/mesh_utils.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/trig.h"
#include "tbx/types/vectors.h"

namespace tbx
{
    struct CapsuleRing
    {
        float y = 0.0F;
        float radius = 0.0F;
    };

    VertexBuffer make_vertex_buffer(const std::vector<Vertex>& vertices)
    {
        return VertexBuffer(vertices, get_default_vertex_buffer_layout());
    }

    Vec3 get_fallback_tangent(const Vec3& normal)
    {
        const auto up = Vec3(0.0F, 1.0F, 0.0F);
        const auto right = Vec3(1.0F, 0.0F, 0.0F);
        auto tangent = cross(up, normal);
        const float tangent_length_squared = dot(tangent, tangent);
        if (tangent_length_squared <= 0.000001F)
            tangent = cross(right, normal);
        return normalize_or_zero(tangent);
    }

    void compute_tangents(std::vector<Vertex>& vertices, const IndexBuffer& indices)
    {
        auto tangent_sums = std::vector<Vec3>(vertices.size(), Vec3(0.0F));
        auto bitangent_sums = std::vector<Vec3>(vertices.size(), Vec3(0.0F));

        for (size_t triangle_index = 0U; triangle_index + 2U < indices.size();
             triangle_index += 3U)
        {
            const auto index0 = static_cast<size_t>(indices[triangle_index + 0U]);
            const auto index1 = static_cast<size_t>(indices[triangle_index + 1U]);
            const auto index2 = static_cast<size_t>(indices[triangle_index + 2U]);
            if (index0 >= vertices.size() || index1 >= vertices.size() || index2 >= vertices.size())
                continue;

            const auto& vertex0 = vertices[index0];
            const auto& vertex1 = vertices[index1];
            const auto& vertex2 = vertices[index2];

            const auto edge0 = vertex1.position - vertex0.position;
            const auto edge1 = vertex2.position - vertex0.position;
            const auto uv_delta0 = vertex1.uv - vertex0.uv;
            const auto uv_delta1 = vertex2.uv - vertex0.uv;

            const float determinant = (uv_delta0.x * uv_delta1.y) - (uv_delta1.x * uv_delta0.y);
            if (determinant >= -0.000001F && determinant <= 0.000001F)
                continue;

            const float inverse_determinant = 1.0F / determinant;
            const auto tangent =
                ((edge0 * uv_delta1.y) - (edge1 * uv_delta0.y)) * inverse_determinant;
            const auto bitangent =
                ((edge1 * uv_delta0.x) - (edge0 * uv_delta1.x)) * inverse_determinant;

            tangent_sums[index0] += tangent;
            tangent_sums[index1] += tangent;
            tangent_sums[index2] += tangent;
            bitangent_sums[index0] += bitangent;
            bitangent_sums[index1] += bitangent;
            bitangent_sums[index2] += bitangent;
        }

        for (size_t vertex_index = 0U; vertex_index < vertices.size(); ++vertex_index)
        {
            const auto normal = normalize_or_zero(vertices[vertex_index].normal);
            auto tangent =
                tangent_sums[vertex_index] - (normal * dot(normal, tangent_sums[vertex_index]));
            tangent = normalize_or_zero(tangent);
            if (dot(tangent, tangent) <= 0.000001F)
                tangent = get_fallback_tangent(normal);

            const auto bitangent = bitangent_sums[vertex_index];
            const float handedness =
                dot(cross(normal, tangent), bitangent) < 0.0F ? -1.0F : 1.0F;
            vertices[vertex_index].tangent = Vec4(tangent.x, tangent.y, tangent.z, handedness);
        }
    }

    Mesh make_uv_sphere_mesh(float radius, uint32 stacks, uint32 sectors)
    {
        auto vertices = std::vector<Vertex> {};
        auto indices = IndexBuffer {};

        const uint32 ring_vertex_count = sectors + 1U;
        vertices.reserve(static_cast<size_t>(ring_vertex_count) * static_cast<size_t>(stacks + 1U));
        indices.reserve(static_cast<size_t>(stacks) * static_cast<size_t>(sectors) * 6U);

        for (uint32 stack_index = 0U; stack_index <= stacks; ++stack_index)
        {
            const float stack_ratio = static_cast<float>(stack_index) / static_cast<float>(stacks);
            const float stack_angle = tbx::PI * stack_ratio;
            const float y = tbx::cos(stack_angle);
            const float ring_radius = tbx::sin(stack_angle);

            for (uint32 sector_index = 0U; sector_index <= sectors; ++sector_index)
            {
                const float sector_ratio =
                    static_cast<float>(sector_index) / static_cast<float>(sectors);
                const float sector_angle = tbx::PI * 2.0F * sector_ratio;

                const float x = ring_radius * tbx::cos(sector_angle);
                const float z = ring_radius * tbx::sin(sector_angle);

                const Vec3 unit_position = Vec3(x, y, z);
                vertices.push_back(
                    Vertex {
                        unit_position * radius,
                        normalize_or_zero(unit_position),
                        Vec2(sector_ratio, 1.0F - stack_ratio),
                        Color(0.0F, 0.0F, 0.0F, 1.0F),
                    });
            }
        }

        for (uint32 stack_index = 0U; stack_index < stacks; ++stack_index)
        {
            const uint32 current_ring = stack_index * ring_vertex_count;
            const uint32 next_ring = (stack_index + 1U) * ring_vertex_count;

            for (uint32 sector_index = 0U; sector_index < sectors; ++sector_index)
            {
                const uint32 current = current_ring + sector_index;
                const uint32 next = next_ring + sector_index;

                if (stack_index != 0U)
                {
                    indices.push_back(current);
                    indices.push_back(current + 1U);
                    indices.push_back(next);
                }

                if (stack_index != stacks - 1U)
                {
                    indices.push_back(current + 1U);
                    indices.push_back(next + 1U);
                    indices.push_back(next);
                }
            }
        }

        compute_tangents(vertices, indices);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, indices);
        update_mesh_bounds(mesh);
        return mesh;
    }

    Mesh make_capsule_mesh(
        float radius,
        float cylinder_half_height,
        uint32 hemisphere_stacks,
        uint32 cylinder_stacks,
        uint32 sectors)
    {
        auto rings = std::vector<CapsuleRing> {};
        rings.reserve(static_cast<size_t>(hemisphere_stacks * 2U + cylinder_stacks + 1U));

        for (uint32 stack_index = 0U; stack_index <= hemisphere_stacks; ++stack_index)
        {
            const float ratio =
                static_cast<float>(stack_index) / static_cast<float>(hemisphere_stacks);
            const float angle = (tbx::PI * 0.5F) * ratio;
            rings.push_back(
                CapsuleRing {
                    .y = cylinder_half_height + tbx::cos(angle) * radius,
                    .radius = tbx::sin(angle) * radius,
                });
        }

        for (uint32 stack_index = 1U; stack_index < cylinder_stacks; ++stack_index)
        {
            const float ratio =
                static_cast<float>(stack_index) / static_cast<float>(cylinder_stacks);
            rings.push_back(
                CapsuleRing {
                    .y = cylinder_half_height - (ratio * 2.0F * cylinder_half_height),
                    .radius = radius,
                });
        }

        for (uint32 stack_index = hemisphere_stacks; stack_index > 0U; --stack_index)
        {
            const float ratio =
                static_cast<float>(stack_index) / static_cast<float>(hemisphere_stacks);
            const float angle = (tbx::PI * 0.5F) * ratio;
            rings.push_back(
                CapsuleRing {
                    .y = -cylinder_half_height - tbx::cos(angle) * radius,
                    .radius = tbx::sin(angle) * radius,
                });
        }

        rings.push_back(
            CapsuleRing {
                .y = -cylinder_half_height - radius,
                .radius = 0.0F,
            });

        const uint32 ring_count = static_cast<uint32>(rings.size());
        const uint32 ring_vertex_count = sectors + 1U;

        auto vertices = std::vector<Vertex> {};
        auto indices = IndexBuffer {};
        vertices.reserve(static_cast<size_t>(ring_count) * static_cast<size_t>(ring_vertex_count));
        indices.reserve(static_cast<size_t>(ring_count - 1U) * static_cast<size_t>(sectors) * 6U);

        const float total_half_height = cylinder_half_height + radius;
        const float total_height = total_half_height * 2.0F;

        for (uint32 ring_index = 0U; ring_index < ring_count; ++ring_index)
        {
            const CapsuleRing ring = rings[ring_index];
            const float v = (ring.y + total_half_height) / total_height;

            for (uint32 sector_index = 0U; sector_index <= sectors; ++sector_index)
            {
                const float u = static_cast<float>(sector_index) / static_cast<float>(sectors);
                const float angle = tbx::PI * 2.0F * u;

                const float x = ring.radius * tbx::cos(angle);
                const float z = ring.radius * tbx::sin(angle);
                const Vec3 position = Vec3(x, ring.y, z);

                Vec3 normal = Vec3(0.0F, 0.0F, 0.0F);
                if (ring.y > cylinder_half_height)
                {
                    normal = normalize_or_zero(Vec3(x, ring.y - cylinder_half_height, z));
                }
                else if (ring.y < -cylinder_half_height)
                {
                    normal = normalize_or_zero(Vec3(x, ring.y + cylinder_half_height, z));
                }
                else
                {
                    normal = normalize_or_zero(Vec3(x, 0.0F, z));
                }

                vertices.push_back(
                    Vertex {
                        position,
                        normal,
                        Vec2(u, 1.0F - v),
                        Color(0.0F, 0.0F, 0.0F, 1.0F),
                    });
            }
        }

        for (uint32 ring_index = 0U; ring_index + 1U < ring_count; ++ring_index)
        {
            const uint32 current_ring = ring_index * ring_vertex_count;
            const uint32 next_ring = (ring_index + 1U) * ring_vertex_count;

            for (uint32 sector_index = 0U; sector_index < sectors; ++sector_index)
            {
                const uint32 current = current_ring + sector_index;
                const uint32 next = next_ring + sector_index;

                indices.push_back(current);
                indices.push_back(next);
                indices.push_back(current + 1U);

                indices.push_back(current + 1U);
                indices.push_back(next);
                indices.push_back(next + 1U);
            }
        }

        compute_tangents(vertices, indices);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, indices);
        update_mesh_bounds(mesh);
        return mesh;
    }
}
