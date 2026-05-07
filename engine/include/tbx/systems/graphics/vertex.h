#pragma once
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/color.h"
#include "tbx/systems/math/vectors.h"
#include "tbx/tbx_api.h"
#include "tbx/types/typedefs.h"
#include <utility>
#include <variant>
#include <vector>

namespace tbx
{
    ///////////// VERTEX DATA //////////////////
    using VertexData = std::variant<int, float, Vec2, Vec3, Vec4, Color>;

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
        else if (std::holds_alternative<Vec4>(data))
        {
            return 4;
        }
        else if (std::holds_alternative<Color>(data))
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
        else if (std::holds_alternative<Vec4>(data))
        {
            return 4 * 4;
        }
        else if (std::holds_alternative<Color>(data))
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
        Color color = {1.0f, 1.0f, 1.0f, 1.0f};
        // (tx, ty, tz, handedness) for tangent-space normal mapping
        Vec4 tangent = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
    };

    ///////////// VERTEX BUFFER //////////////////
    enum class VertexAttributeSemantic
    {
        NONE,
        POSITION,
        COLOR,
        NORMAL,
        UV,
        TANGENT
    };

    struct TBX_API VertexLayoutElement
    {
        VertexAttributeSemantic semantic = VertexAttributeSemantic::NONE;
        VertexData type = 0;
        bool normalized = false;
    };

    struct TBX_API VertexBufferAttribute
    {
        VertexAttributeSemantic semantic = VertexAttributeSemantic::NONE;
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
            static constexpr VertexAttributeSemantic default_semantics[] = {
                VertexAttributeSemantic::POSITION,
                VertexAttributeSemantic::COLOR,
                VertexAttributeSemantic::NORMAL,
                VertexAttributeSemantic::UV,
                VertexAttributeSemantic::TANGENT,
            };

            for (uint32 index = 0U; index < static_cast<uint32>(layout.size()); ++index)
            {
                const auto& value = layout[static_cast<size>(index)];
                VertexBufferAttribute attribute = {};
                attribute.semantic = index < 5U ? default_semantics[index]
                                                : VertexAttributeSemantic::NONE;
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
        VertexBufferLayout(const std::vector<VertexLayoutElement>& layout)
        {
            auto attributes = std::vector<VertexBufferAttribute>();
            uint32 current_offset = 0;
            for (const auto& value : layout)
            {
                VertexBufferAttribute attribute = {};
                attribute.semantic = value.semantic;
                attribute.type = value.type;
                attribute.size = get_vertex_data_size(value.type);
                attribute.count = get_vertex_data_count(value.type);
                attribute.offset = current_offset;
                attribute.normalized = value.normalized;
                current_offset += attribute.size;
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
                    .semantic = VertexAttributeSemantic::POSITION,
                    .type = Vec3(),
                },
                VertexLayoutElement {
                    .semantic = VertexAttributeSemantic::COLOR,
                    .type = Color(),
                },
                VertexLayoutElement {
                    .semantic = VertexAttributeSemantic::NORMAL,
                    .type = Vec3(),
                },
                VertexLayoutElement {
                    .semantic = VertexAttributeSemantic::UV,
                    .type = Vec2(),
                },
                VertexLayoutElement {
                    .semantic = VertexAttributeSemantic::TANGENT,
                    .type = Vec4(),
                },
            });
    }

    inline Vec4 get_vertex_attribute_value(
        const Vertex& vertex,
        const VertexAttributeSemantic semantic)
    {
        switch (semantic)
        {
            case VertexAttributeSemantic::POSITION:
                return Vec4(vertex.position, 0.0F);
            case VertexAttributeSemantic::COLOR:
                return Vec4(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
            case VertexAttributeSemantic::NORMAL:
                return Vec4(vertex.normal, 0.0F);
            case VertexAttributeSemantic::UV:
                return Vec4(vertex.uv, 0.0F, 0.0F);
            case VertexAttributeSemantic::TANGENT:
                return vertex.tangent;
            case VertexAttributeSemantic::NONE:
            default:
                return Vec4(0.0F);
        }
    }

    inline void append_vertex_attribute(
        const Vertex& vertex,
        const VertexBufferAttribute& attribute,
        std::vector<float>& out_values)
    {
        const Vec4 value = get_vertex_attribute_value(vertex, attribute.semantic);
        for (uint32 component = 0U; component < attribute.count; ++component)
            out_values.push_back(value[component]);
    }

    inline std::vector<float> flatten_vertex_vector(
        const std::vector<Vertex>& vertices,
        const VertexBufferLayout& layout)
    {
        auto mesh_points = std::vector<float>();
        mesh_points.reserve(
            vertices.size() * static_cast<size>(layout.stride / static_cast<uint32>(sizeof(float))));

        for (const auto& vertex : vertices)
            for (const auto& attribute : layout.elements)
                append_vertex_attribute(vertex, attribute, mesh_points);

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
