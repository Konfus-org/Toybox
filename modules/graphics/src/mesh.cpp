#include "tbx/graphics/mesh.h"
#include "tbx/math/vectors.h"
#include <array>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace tbx
{
    namespace
    {
        static VertexBuffer make_vertex_buffer(const std::vector<Vertex>& vertices)
        {
            return VertexBuffer(
                vertices,
                {{
                    Vec3(0.0F),
                    RgbaColor(),
                    Vec3(0.0F),
                    Vec2(0.0F),
                }});
        }

        static Vec3 normalize_or_zero(const Vec3& value)
        {
            const float length_squared = value.x * value.x + value.y * value.y + value.z * value.z;
            if (length_squared <= 0.0F)
                return Vec3(0.0F, 0.0F, 0.0F);

            return normalize(value);
        }

        static Mesh make_uv_sphere_mesh(float radius, uint32 stacks, uint32 sectors)
        {
            auto vertices = std::vector<Vertex> {};
            auto indices = IndexBuffer {};

            const uint32 ring_vertex_count = sectors + 1U;
            vertices.reserve(
                static_cast<size_t>(ring_vertex_count) * static_cast<size_t>(stacks + 1U));
            indices.reserve(static_cast<size_t>(stacks) * static_cast<size_t>(sectors) * 6U);

            for (uint32 stack_index = 0U; stack_index <= stacks; ++stack_index)
            {
                const float stack_ratio =
                    static_cast<float>(stack_index) / static_cast<float>(stacks);
                const float stack_angle = std::numbers::pi_v<float> * stack_ratio;
                const float y = std::cos(stack_angle);
                const float ring_radius = std::sin(stack_angle);

                for (uint32 sector_index = 0U; sector_index <= sectors; ++sector_index)
                {
                    const float sector_ratio =
                        static_cast<float>(sector_index) / static_cast<float>(sectors);
                    const float sector_angle = std::numbers::pi_v<float> * 2.0F * sector_ratio;

                    const float x = ring_radius * std::cos(sector_angle);
                    const float z = ring_radius * std::sin(sector_angle);

                    const Vec3 unit_position = Vec3(x, y, z);
                    vertices.push_back(
                        Vertex {
                            unit_position * radius,
                            normalize_or_zero(unit_position),
                            Vec2(sector_ratio, 1.0F - stack_ratio),
                            RgbaColor(0.0F, 0.0F, 0.0F, 1.0F),
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
                        indices.push_back(next);
                        indices.push_back(current + 1U);
                    }

                    if (stack_index != stacks - 1U)
                    {
                        indices.push_back(current + 1U);
                        indices.push_back(next);
                        indices.push_back(next + 1U);
                    }
                }
            }

            const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
            return Mesh(vertex_buffer, indices);
        }

        struct CapsuleRing
        {
            float y = 0.0F;
            float radius = 0.0F;
        };

        static Mesh make_capsule_mesh(
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
                const float angle = (std::numbers::pi_v<float> * 0.5F) * ratio;
                rings.push_back(
                    CapsuleRing {
                        .y = cylinder_half_height + std::cos(angle) * radius,
                        .radius = std::sin(angle) * radius,
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
                const float angle = (std::numbers::pi_v<float> * 0.5F) * ratio;
                rings.push_back(
                    CapsuleRing {
                        .y = -cylinder_half_height - std::cos(angle) * radius,
                        .radius = std::sin(angle) * radius,
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
            vertices.reserve(
                static_cast<size_t>(ring_count) * static_cast<size_t>(ring_vertex_count));
            indices.reserve(
                static_cast<size_t>(ring_count - 1U) * static_cast<size_t>(sectors) * 6U);

            const float total_half_height = cylinder_half_height + radius;
            const float total_height = total_half_height * 2.0F;

            for (uint32 ring_index = 0U; ring_index < ring_count; ++ring_index)
            {
                const CapsuleRing ring = rings[ring_index];
                const float v = (ring.y + total_half_height) / total_height;

                for (uint32 sector_index = 0U; sector_index <= sectors; ++sector_index)
                {
                    const float u = static_cast<float>(sector_index) / static_cast<float>(sectors);
                    const float angle = std::numbers::pi_v<float> * 2.0F * u;

                    const float x = ring.radius * std::cos(angle);
                    const float z = ring.radius * std::sin(angle);
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
                            RgbaColor(0.0F, 0.0F, 0.0F, 1.0F),
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

            const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
            return Mesh(vertex_buffer, indices);
        }
    }

    Mesh::Mesh()
    {
        Mesh default_quad = make_quad();
        vertices = default_quad.vertices;
        indices = default_quad.indices;
    }

    Mesh::Mesh(VertexBuffer vert_buff, IndexBuffer index_buff)
        : vertices(std::move(vert_buff))
        , indices(std::move(index_buff))
    {
    }

    Mesh make_triangle()
    {
        const std::vector<Vertex> triangle_mesh_vertices = {
            Vertex {
                Vec3(-0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.0F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)}};

        const IndexBuffer index_buffer = {0, 1, 2};
        const VertexBuffer vertex_buffer = make_vertex_buffer(triangle_mesh_vertices);

        return Mesh(vertex_buffer, index_buffer);
    }

    Mesh make_quad()
    {
        const std::vector<Vertex> quad_mesh_vertices = {
            Vertex {
                Vec3(-0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 0.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 1.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(-0.5F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 1.0F),
                RgbaColor(0.0F, 0.0F, 0.0F, 1.0F)}};

        const IndexBuffer index_buffer = {0, 1, 2, 2, 3, 0};
        const VertexBuffer vertex_buffer = make_vertex_buffer(quad_mesh_vertices);

        return Mesh(vertex_buffer, index_buffer);
    }

    Mesh make_cube()
    {
        const std::array<Vec3, 8> positions = {
            Vec3(-0.5F, -0.5F, -0.5F),
            Vec3(0.5F, -0.5F, -0.5F),
            Vec3(0.5F, 0.5F, -0.5F),
            Vec3(-0.5F, 0.5F, -0.5F),
            Vec3(-0.5F, -0.5F, 0.5F),
            Vec3(0.5F, -0.5F, 0.5F),
            Vec3(0.5F, 0.5F, 0.5F),
            Vec3(-0.5F, 0.5F, 0.5F),
        };

        const std::array<Vec3, 6> normals = {
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(-1.0F, 0.0F, 0.0F),
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
        };

        const std::array<std::array<uint32, 4>, 6> faces = {
            std::array<uint32, 4> {4, 5, 6, 7},
            std::array<uint32, 4> {1, 0, 3, 2},
            std::array<uint32, 4> {0, 4, 7, 3},
            std::array<uint32, 4> {5, 1, 2, 6},
            std::array<uint32, 4> {3, 7, 6, 2},
            std::array<uint32, 4> {0, 1, 5, 4},
        };

        const std::array<Vec2, 4> uvs = {
            Vec2(0.0F, 0.0F),
            Vec2(1.0F, 0.0F),
            Vec2(1.0F, 1.0F),
            Vec2(0.0F, 1.0F),
        };

        auto vertices = std::vector<Vertex> {};
        auto indices = IndexBuffer {};
        vertices.reserve(24U);
        indices.reserve(36U);

        for (size_t face_index = 0U; face_index < faces.size(); ++face_index)
        {
            const uint32 base_vertex = static_cast<uint32>(vertices.size());
            for (uint32 corner = 0U; corner < 4U; ++corner)
            {
                vertices.push_back(
                    Vertex {
                        positions[faces[face_index][corner]],
                        normals[face_index],
                        uvs[corner],
                        RgbaColor(0.0F, 0.0F, 0.0F, 1.0F),
                    });
            }

            indices.push_back(base_vertex + 0U);
            indices.push_back(base_vertex + 1U);
            indices.push_back(base_vertex + 2U);
            indices.push_back(base_vertex + 2U);
            indices.push_back(base_vertex + 3U);
            indices.push_back(base_vertex + 0U);
        }

        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        return Mesh(vertex_buffer, indices);
    }

    Mesh make_sphere()
    {
        constexpr float radius = 0.5F;
        constexpr uint32 stacks = 16U;
        constexpr uint32 sectors = 24U;
        return make_uv_sphere_mesh(radius, stacks, sectors);
    }

    Mesh make_capsule()
    {
        constexpr float radius = 0.25F;
        constexpr float cylinder_half_height = 0.25F;
        constexpr uint32 hemisphere_stacks = 8U;
        constexpr uint32 cylinder_stacks = 8U;
        constexpr uint32 sectors = 24U;

        return make_capsule_mesh(
            radius,
            cylinder_half_height,
            hemisphere_stacks,
            cylinder_stacks,
            sectors);
    }
}
