#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/color.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vectors.h"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tbx
{
    ///////////// VERTEX DATA //////////////////
    using VertexData = GraphicsVertexFormat;

    inline int32 get_vertex_data_count(const VertexData& data)
    {
        if (data == GraphicsVertexFormat::VEC2)
        {
            return 2;
        }
        else if (data == GraphicsVertexFormat::VEC3)
        {
            return 3;
        }
        else if (data == GraphicsVertexFormat::VEC4)
        {
            return 4;
        }
        else if (data == GraphicsVertexFormat::FLOAT)
        {
            return 1;
        }
        else if (data == GraphicsVertexFormat::UINT32 || data == GraphicsVertexFormat::INT32)
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
        if (data == GraphicsVertexFormat::VEC2)
        {
            return 4 * 2;
        }
        else if (data == GraphicsVertexFormat::VEC3)
        {
            return 4 * 3;
        }
        else if (data == GraphicsVertexFormat::VEC4)
        {
            return 4 * 4;
        }
        else if (data == GraphicsVertexFormat::FLOAT)
        {
            return 4;
        }
        else if (data == GraphicsVertexFormat::UINT32 || data == GraphicsVertexFormat::INT32)
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
        Color color = {1.0f, 1.0f, 1.0f, 1.0f};
        // (tx, ty, tz, handedness) for tangent-space normal mapping
        Vec4 tangent = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    };

    ///////////// VERTEX BUFFER //////////////////

    inline constexpr const char* vertex_attribute_position_debug_name = "position";
    inline constexpr const char* vertex_attribute_color_debug_name = "color";
    inline constexpr const char* vertex_attribute_normal_debug_name = "normal";
    inline constexpr const char* vertex_attribute_uv_debug_name = "uv";
    inline constexpr const char* vertex_attribute_tangent_debug_name = "tangent";

    struct TBX_API VertexLayoutElement
    {
        std::string debug_name = {};
        VertexData type = GraphicsVertexFormat::FLOAT;
        bool normalized = false;
    };

    struct TBX_API VertexBufferAttribute
    {
        std::string debug_name = {};
        VertexData type = GraphicsVertexFormat::FLOAT;
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
            attributes.reserve(layout.size());
            uint32 current_offset = 0;
            static constexpr const char* default_debug_names[] = {
                vertex_attribute_position_debug_name,
                vertex_attribute_color_debug_name,
                vertex_attribute_normal_debug_name,
                vertex_attribute_uv_debug_name,
                vertex_attribute_tangent_debug_name,
            };

            for (uint32 index = 0U; index < static_cast<uint32>(layout.size()); ++index)
            {
                const auto& value = layout[static_cast<size>(index)];
                VertexBufferAttribute attribute = {};
                attribute.debug_name =
                    index < 5U ? std::string(default_debug_names[index]) : std::string();
                attribute.type = value;
                attribute.offset = current_offset;
                attribute.normalized = false;
                current_offset += static_cast<uint32>(get_vertex_data_size(value));
                attributes.push_back(attribute);
            }
            stride = current_offset;
            elements = attributes;
        }
        VertexBufferLayout(const std::vector<VertexLayoutElement>& layout)
        {
            auto attributes = std::vector<VertexBufferAttribute>();
            attributes.reserve(layout.size());
            uint32 current_offset = 0;
            for (const auto& value : layout)
            {
                VertexBufferAttribute attribute = {};
                attribute.debug_name = value.debug_name;
                attribute.type = value.type;
                attribute.offset = current_offset;
                attribute.normalized = value.normalized;
                current_offset += static_cast<uint32>(get_vertex_data_size(value.type));
                attributes.push_back(attribute);
            }
            stride = current_offset;
            elements = attributes;
        }

        std::vector<VertexBufferAttribute> elements = {};
        uint32 stride = 0;
    };

    inline VertexBufferLayout get_default_vertex_buffer_layout()
    {
        return VertexBufferLayout(
            std::vector<VertexLayoutElement> {
                VertexLayoutElement {
                    .debug_name = vertex_attribute_position_debug_name,
                    .type = GraphicsVertexFormat::VEC3,
                },
                VertexLayoutElement {
                    .debug_name = vertex_attribute_color_debug_name,
                    .type = GraphicsVertexFormat::VEC4,
                },
                VertexLayoutElement {
                    .debug_name = vertex_attribute_normal_debug_name,
                    .type = GraphicsVertexFormat::VEC3,
                },
                VertexLayoutElement {
                    .debug_name = vertex_attribute_uv_debug_name,
                    .type = GraphicsVertexFormat::VEC2,
                },
                VertexLayoutElement {
                    .debug_name = vertex_attribute_tangent_debug_name,
                    .type = GraphicsVertexFormat::VEC4,
                },
            });
    }

    inline Vec4 get_vertex_attribute_value(
        const Vertex& vertex,
        const std::string_view debug_name)
    {
        if (debug_name == vertex_attribute_position_debug_name)
            return Vec4(vertex.position, 0.0F);
        if (debug_name == vertex_attribute_color_debug_name)
            return Vec4(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
        if (debug_name == vertex_attribute_normal_debug_name)
            return Vec4(vertex.normal, 0.0F);
        if (debug_name == vertex_attribute_uv_debug_name)
            return Vec4(vertex.uv, 0.0F, 0.0F);
        if (debug_name == vertex_attribute_tangent_debug_name)
            return vertex.tangent;

        return Vec4(0.0F);
    }

    inline void append_vertex_attribute(
        const Vertex& vertex,
        const VertexBufferAttribute& attribute,
        std::vector<float>& out_values)
    {
        const Vec4 value = get_vertex_attribute_value(vertex, attribute.debug_name);
        const uint32 component_count =
            static_cast<uint32>(get_vertex_data_count(attribute.type));
        for (uint32 component = 0U; component < component_count; ++component)
            out_values.push_back(value[component]);
    }

    inline std::vector<float> flatten_vertex_vector(
        const std::vector<Vertex>& vertices,
        const VertexBufferLayout& layout)
    {
        auto mesh_points = std::vector<float>();
        mesh_points.reserve(
            vertices.size()
            * static_cast<size>(layout.stride / static_cast<uint32>(sizeof(float))));

        const uint32 attribute_count = static_cast<uint32>(layout.elements.size());
        for (const auto& vertex : vertices)
            for (uint32 attribute_index = 0U; attribute_index < attribute_count; ++attribute_index)
            {
                append_vertex_attribute(
                    vertex,
                    layout.elements[static_cast<size>(attribute_index)],
                    mesh_points);
            }

        return mesh_points;
    }

    inline std::vector<float> flatten_vertex_vector(const std::vector<Vertex>& vertices)
    {
        return flatten_vertex_vector(vertices, get_default_vertex_buffer_layout());
    }

    struct TBX_API VertexBuffer
    {
        VertexBuffer() = default;
        VertexBuffer(const std::vector<Vertex>& vertices, VertexBufferLayout layout)
            : vertices(flatten_vertex_vector(vertices, layout))
            , layout(std::move(layout))
        {
        }

        bool empty() const
        {
            return vertices.empty();
        }

        auto begin() const
        {
            return vertices.begin();
        }

        auto end() const
        {
            return vertices.end();
        }

        size_t size() const
        {
            return vertices.size();
        }

        auto data() const
        {
            return vertices.data();
        }

        std::vector<float> vertices = {};
        VertexBufferLayout layout = {};
    };
}
