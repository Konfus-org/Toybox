#include "tbx/graphics/mesh.h"
#include <utility>
#include <vector>

namespace tbx
{
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
        const std::vector<Vertex> TRIANGLE_MESH_VERTICES = {
            Vertex {
                Vec3(-0.5f, -0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            Vertex {
                Vec3(0.5f, -0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            Vertex {
                Vec3(0.0f, 0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)}};

        const IndexBuffer INDEX_BUFFER = {0, 1, 2};
        const VertexBuffer VERTEX_BUFFER = {
            TRIANGLE_MESH_VERTICES,
            {{
                Vec3(0.0f),
                RgbaColor(),
                Vec3(0.0f),
                Vec2(0.0f),
            }}};

        return {VERTEX_BUFFER, INDEX_BUFFER};
    }

    Mesh make_quad()
    {
        const std::vector<Vertex> QUAD_MESH_VERTICES = {
            Vertex {
                Vec3(-0.5f, -0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(0.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            Vertex {
                Vec3(0.5f, -0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(1.0f, 0.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            Vertex {
                Vec3(0.5f, 0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(1.0f, 1.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)},
            Vertex {
                Vec3(-0.5f, 0.5f, 0.0f),
                Vec3(0.0f, 0.0f, 1.0f),
                Vec2(0.0f, 1.0f),
                RgbaColor(0.0f, 0.0f, 0.0f, 1.0f)}};

        const IndexBuffer INDEX_BUFFER = {0, 1, 2, 2, 3, 0};
        const VertexBuffer VERTEX_BUFFER = {
            QUAD_MESH_VERTICES,
            {{
                Vec3(0.0f),
                RgbaColor(),
                Vec3(0.0f),
                Vec2(0.0f),
            }}};

        return {VERTEX_BUFFER, INDEX_BUFFER};
    }
}
