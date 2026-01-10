#pragma once
#include "tbx/common/int.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/color.h"
#include "tbx/math/vectors.h"
#include "tbx/tbx_api.h"
#include <variant>
#include <vector>

namespace tbx
{
    ///////////// VERTEX DATA //////////////////

    using VertexData = std::variant<int, float, Vec2, Vec3, RgbaColor>;

    inline int32 get_vertex_data_count(const VertexData& data)
    {
        if (std::holds_alternative<Vec2>(data))
        {
            return 2;
        }
        else if (std::holds_alternative<Vec3>(data))
        {
            return 3;
        }
        else if (std::holds_alternative<RgbaColor>(data))
        {
            return 4;
        }
        else if (std::holds_alternative<float>(data))
        {
            return 1;
        }
        else if (std::holds_alternative<int>(data))
        {
            return 1;
        }
        else
        {
            TBX_ASSERT(false, "Could not get vert data count, given unkown type.");
            return 0;
        }
    }

    inline int32 get_vertex_data_size(const VertexData& data)
    {
        if (std::holds_alternative<Vec2>(data))
        {
            return 4 * 2;
        }
        else if (std::holds_alternative<Vec3>(data))
        {
            return 4 * 3;
        }
        else if (std::holds_alternative<RgbaColor>(data))
        {
            return 4 * 4;
        }
        else if (std::holds_alternative<float>(data))
        {
            return 4;
        }
        else if (std::holds_alternative<int>(data))
        {
            return 4;
        }
        else
        {
            TBX_ASSERT(false, "Could not get vertex data size, given unkown type.");
            return 0;
        }
    }

    ///////////// VERTEX //////////////////

    struct TBX_API Vertex
    {
        // (x, y, z) in 3D space
        Vec3 position = Vec3(0.0f);
        // (nx, ny, nz) for lighting
        Vec3 normal = Vec3(0.0f);
        // Texture coordinate for texture mapping
        Vec2 uv = Vec2(0.0f);
        // (r, g, b, a) for color
        RgbaColor color = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    ///////////// VERTEX BUFFER //////////////////

    inline std::vector<float> flatten_vertex_vector(const std::vector<Vertex>& vertices)
    {
        const auto vertex_count = vertices.size();
        auto mesh_points = std::vector<float>(vertex_count * 12);
        int write_index = 0;

        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.position;
            mesh_points[write_index] = position.x;
            mesh_points[write_index + 1] = position.y;
            mesh_points[write_index + 2] = position.z;

            const auto& vertex_color = vertex.color;
            mesh_points[write_index + 3] = vertex_color.r;
            mesh_points[write_index + 4] = vertex_color.g;
            mesh_points[write_index + 5] = vertex_color.b;
            mesh_points[write_index + 6] = vertex_color.a;

            const auto& vertex_normal = vertex.normal;
            mesh_points[write_index + 7] = vertex_normal.x;
            mesh_points[write_index + 8] = vertex_normal.y;
            mesh_points[write_index + 9] = vertex_normal.z;

            const auto& texture_coord = vertex.uv;
            mesh_points[write_index + 10] = texture_coord.x;
            mesh_points[write_index + 11] = texture_coord.y;

            write_index += 12;
        }

        return mesh_points;
    }

    struct TBX_API VertexBufferAttribute
    {
        VertexData type = 0;
        uint32 size = 0;
        uint32 count = 0;
        uint32 offset = 0;
        bool normalized = false;
    };

    // Used to describe the layout of a vertex buffer.
    // I.e. does a vertex have position and color? Other properties?
    struct TBX_API VertexBufferLayout
    {
        VertexBufferLayout() = default;
        VertexBufferLayout(const std::vector<VertexData>& layout)
        {
            auto attributes = std::vector<VertexBufferAttribute>();
            uint32 current_offset = 0;
            for (const auto& value : layout)
            {
                VertexBufferAttribute attribute = {};
                attribute.type = value;
                attribute.size = get_vertex_data_size(value);
                attribute.count = get_vertex_data_count(value);
                attribute.offset = current_offset;
                attribute.normalized = false;
                current_offset += attribute.size;
                attributes.push_back(attribute);
            }
            stride = current_offset;
            elements = attributes;
        }

        std::vector<VertexBufferAttribute> elements = {};
        uint32 stride = 0;
    };

    struct TBX_API VertexBuffer
    {
        VertexBuffer() = default;
        VertexBuffer(const std::vector<Vertex>& vertices, const VertexBufferLayout& layout)
            : vertices(flatten_vertex_vector(vertices))
            , layout(layout)
        {
        }

        std::vector<float> vertices = {};
        VertexBufferLayout layout = {};
    };
}
