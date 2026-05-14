#include "tbx/types/components/mesh.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/mesh_utils.h"
#include "tbx/types/trig.h"
#include <array>
#include <utility>
#include <vector>

namespace tbx
{
    Mesh::Mesh()
    {
        Mesh default_quad = make_quad();
        vertices = default_quad.vertices;
        indices = default_quad.indices;
        bounds = default_quad.bounds;
    }

    Mesh::Mesh(VertexBuffer vert_buff, IndexBuffer index_buff)
        : vertices(std::move(vert_buff))
        , indices(std::move(index_buff))
    {
        update_mesh_bounds(*this);
    }

    Mesh make_triangle()
    {
        const std::vector<Vertex> triangle_mesh_vertices = {
            Vertex {
                Vec3(-0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.0F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)}};

        const IndexBuffer index_buffer = {0, 1, 2};
        auto vertices = triangle_mesh_vertices;
        compute_tangents(vertices, index_buffer);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, index_buffer);
        update_mesh_bounds(mesh);
        return mesh;
    }

    Mesh make_quad()
    {
        const std::vector<Vertex> quad_mesh_vertices = {
            Vertex {
                Vec3(-0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, -0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(0.5F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 1.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(-0.5F, 0.5F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 1.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)}};

        const IndexBuffer index_buffer = {0, 1, 2, 2, 3, 0};
        auto vertices = quad_mesh_vertices;
        compute_tangents(vertices, index_buffer);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, index_buffer);
        update_mesh_bounds(mesh);
        return mesh;
    }

    Mesh make_fullscreen_quad()
    {
        const std::vector<Vertex> fullscreen_quad_vertices = {
            Vertex {
                Vec3(-1.0F, -1.0F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(1.0F, -1.0F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 0.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(1.0F, 1.0F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(1.0F, 1.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)},
            Vertex {
                Vec3(-1.0F, 1.0F, 0.0F),
                Vec3(0.0F, 0.0F, 1.0F),
                Vec2(0.0F, 1.0F),
                Color(0.0F, 0.0F, 0.0F, 1.0F)}};

        const IndexBuffer index_buffer = {0, 1, 2, 2, 3, 0};
        auto vertices = fullscreen_quad_vertices;
        compute_tangents(vertices, index_buffer);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, index_buffer);
        update_mesh_bounds(mesh);
        return mesh;
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
                        Color(0.0F, 0.0F, 0.0F, 1.0F),
                    });
            }

            indices.push_back(base_vertex + 0U);
            indices.push_back(base_vertex + 1U);
            indices.push_back(base_vertex + 2U);
            indices.push_back(base_vertex + 2U);
            indices.push_back(base_vertex + 3U);
            indices.push_back(base_vertex + 0U);
        }

        compute_tangents(vertices, indices);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, indices);
        update_mesh_bounds(mesh);
        return mesh;
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

    Mesh make_sky_dome()
    {
        constexpr uint32 subdivision_count = 24U;
        constexpr auto face_count = size_t {6U};

        const auto face_normals = std::array<Vec3, face_count> {
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(-1.0F, 0.0F, 0.0F),
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, -1.0F, 0.0F),
        };
        const auto face_u_axes = std::array<Vec3, face_count> {
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(-1.0F, 0.0F, 0.0F),
            Vec3(0.0F, 0.0F, 1.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(1.0F, 0.0F, 0.0F),
            Vec3(1.0F, 0.0F, 0.0F),
        };
        const auto face_v_axes = std::array<Vec3, face_count> {
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, 1.0F, 0.0F),
            Vec3(0.0F, 0.0F, -1.0F),
            Vec3(0.0F, 0.0F, 1.0F),
        };

        const uint32 vertices_per_row = subdivision_count + 1U;
        const uint32 vertices_per_face = vertices_per_row * vertices_per_row;
        auto vertices = std::vector<Vertex> {};
        vertices.reserve(static_cast<size_t>(vertices_per_face) * face_count);

        auto indices = IndexBuffer {};
        indices.reserve(
            static_cast<size_t>(subdivision_count) * static_cast<size_t>(subdivision_count) * 6U
            * face_count);

        for (size_t face_index = 0U; face_index < face_count; ++face_index)
        {
            const uint32 face_vertex_offset = static_cast<uint32>(vertices.size());
            const Vec3 face_normal = face_normals[face_index];
            const Vec3 face_u = face_u_axes[face_index];
            const Vec3 face_v = face_v_axes[face_index];

            for (uint32 y = 0U; y <= subdivision_count; ++y)
            {
                const float v = static_cast<float>(y) / static_cast<float>(subdivision_count);
                const float cube_y = (v * 2.0F) - 1.0F;

                for (uint32 x = 0U; x <= subdivision_count; ++x)
                {
                    const float u = static_cast<float>(x) / static_cast<float>(subdivision_count);
                    const float cube_x = (u * 2.0F) - 1.0F;
                    const Vec3 cube_position =
                        face_normal + (face_u * cube_x) + (face_v * cube_y);
                    const Vec3 sphere_position = normalize_or_zero(cube_position);

                    vertices.push_back(
                        Vertex {
                            .position = sphere_position,
                            .normal = sphere_position,
                            .uv = Vec2(u, 1.0F - v),
                        });
                }
            }

            for (uint32 y = 0U; y < subdivision_count; ++y)
            {
                for (uint32 x = 0U; x < subdivision_count; ++x)
                {
                    const uint32 top_left = face_vertex_offset + (y * vertices_per_row) + x;
                    const uint32 top_right = top_left + 1U;
                    const uint32 bottom_left = top_left + vertices_per_row;
                    const uint32 bottom_right = bottom_left + 1U;

                    indices.push_back(top_left);
                    indices.push_back(bottom_left);
                    indices.push_back(top_right);

                    indices.push_back(top_right);
                    indices.push_back(bottom_left);
                    indices.push_back(bottom_right);
                }
            }
        }

        compute_tangents(vertices, indices);
        const VertexBuffer vertex_buffer = make_vertex_buffer(vertices);
        Mesh mesh(vertex_buffer, indices);
        update_mesh_bounds(mesh);
        return mesh;
    }
}
