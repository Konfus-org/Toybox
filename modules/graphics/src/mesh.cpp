#include "tbx/graphics/mesh.h"
#include <vector>

namespace tbx
{
    Mesh Mesh::quad = make_quad();
    Mesh Mesh::triangle = make_triangle();

    Mesh::Mesh()
    {
        const Mesh& default_quad = Mesh::quad;
        vertices = default_quad.vertices;
        indices = default_quad.indices;
    }

    Mesh::Mesh(const VertexBuffer& vertBuff, const IndexBuffer& indexBuff)
        : vertices(vertBuff)
        , indices(indexBuff)
    {
    }

    Mesh make_triangle()
    {
        const std::vector<Vertex> triangle_mesh_vertices =
        {
            Vertex
            {
                math::vec3(-0.5f, -0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            },
            Vertex
            {
                math::vec3(0.5f, -0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            },
            Vertex
            {
                math::vec3(0.0f, 0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            }
        };

        const IndexBuffer index_buffer = { 0, 1, 2 };
        const VertexBuffer vertex_buffer =
        {
            triangle_mesh_vertices,
            {{
                math::vec3(0.0f),
                RgbaColor(),
                math::vec3(0.0f),
                math::vec2(0.0f),
            }}
        };

        return { vertex_buffer, index_buffer };
    }

    Mesh make_quad()
    {
        const std::vector<Vertex> quad_mesh_vertices =
        {
            Vertex
            {
                math::vec3(-0.5f, -0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            },
            Vertex
            {
                math::vec3(0.5f, -0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(1.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            },
            Vertex
            {
                math::vec3(0.5f, 0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(1.0f, 1.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            },
            Vertex
            {
                math::vec3(-0.5f, 0.5f, 0.0f),
                math::vec3(0.0f, 0.0f, 0.0f),
                math::vec2(0.0f, 1.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)
            }
        };

        const IndexBuffer index_buffer = { 0, 1, 2, 2, 3, 0 };
        const VertexBuffer vertex_buffer =
        {
            quad_mesh_vertices,
            {{
                math::vec3(0.0f),
                RgbaColor(),
                math::vec3(0.0f),
                math::vec2(0.0f),
            }}
        };

        return { vertex_buffer, index_buffer };
    }
}
