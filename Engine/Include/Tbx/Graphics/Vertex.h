#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Math/Int.h"
#include <vector>
#include <variant>

namespace Tbx
{
    using VertexData = std::variant<int, float, Vector2, Vector3, RgbaColor>;

    struct TBX_EXPORT Vertex
    {
        // (x, y, z) in 3D space
        Vector3 Position  = { 0, 0, 0 };
        // (nx, ny, nz) for lighting
        Vector3 Normal    = { 0, 0, 0 };
        // Texture coordinate for texture mapping
        Vector2 UV = { 0, 0 };
        // (r, g, b, a) for color
        RgbaColor Color   = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct TBX_EXPORT VertexBufferAttribute
    {
        VertexData Type = -1;
        uint32 Size = 0;
        uint32 Offset = 0;
        bool Normalized = false;
    };

    struct TBX_EXPORT VertexBufferLayout
    {
        VertexBufferLayout() = default;
        VertexBufferLayout(std::vector<VertexData> layout)
        {
            auto elements = std::vector<VertexBufferAttribute>();

            uint32 offset = 0;
            for (auto& type : layout)
            {
                VertexBufferAttribute element = {};
                element.Type = type;
                element.Size = sizeof(type);
                element.Offset = offset;
                element.Normalized = false;
                offset += element.Size;
                elements.push_back(element);
            }
            Stride = offset;
            Elements = elements;
        }

        std::vector<VertexBufferAttribute> Elements = {};
        uint32 Stride = 0;
    };

    inline std::vector<float> ConvertVertexVectorToFloatVector(const std::vector<Vertex>& vertices)
    {
        const auto numberOfVertices = vertices.size();
        auto meshPoints = std::vector<float>(numberOfVertices * 12);
        int positionToPlace = 0;

        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.Position;
            meshPoints[positionToPlace] = position.X;
            meshPoints[positionToPlace + 1] = position.Y;
            meshPoints[positionToPlace + 2] = position.Z;

            const auto& color = vertex.Color;
            meshPoints[positionToPlace + 3] = color.R;
            meshPoints[positionToPlace + 4] = color.G;
            meshPoints[positionToPlace + 5] = color.B;
            meshPoints[positionToPlace + 6] = color.A;

            const auto& normal = vertex.Normal;
            meshPoints[positionToPlace + 7] = normal.X;
            meshPoints[positionToPlace + 8] = normal.Y;
            meshPoints[positionToPlace + 9] = normal.Z;

            const auto& textCoord = vertex.UV;
            meshPoints[positionToPlace + 10] = (float)textCoord.X;
            meshPoints[positionToPlace + 11] = (float)textCoord.Y;

            positionToPlace += 12;
        }

        return meshPoints;
    }

    struct TBX_EXPORT VertexBuffer
    {
        VertexBuffer() = default;
        VertexBuffer(const std::vector<Vertex>& vertices)
            : Vertices(ConvertVertexVectorToFloatVector(vertices)) {}
        VertexBuffer(const std::vector<Vertex>& vertices, VertexBufferLayout layout)
            : Vertices(ConvertVertexVectorToFloatVector(vertices)), Layout(layout) {}

        std::vector<float> Vertices = {};
        VertexBufferLayout Layout =
        {{
            // Pos
            Vector3(),
            // Vert Color
            RgbaColor(),
            // Normal
            Vector3(),
            // Tex Coord
            Vector2(),
        }};
    };
}
