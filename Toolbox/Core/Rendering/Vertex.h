#pragma once
#include "ToolboxAPI.h"
#include "Color.h"
#include "Buffers.h"
#include "Math/Vectors.h"

namespace Tbx
{
    struct TBX_API Vertex
    {
        Vertex() = default;
        explicit(false) Vertex(const Vector3& position) 
            : Position(position) {}
        Vertex(const Vector3& position, const Color& color)
            : Position(position), Color(color) {}
        Vertex(const Vector3& position, const Vector3& normal, const Vector2I& texCoord)
            : Position(position), Normal(normal), TexCoord(texCoord) {}
        Vertex(const Vector3& position, const Vector3& normal, const Vector2I& texCoord, const Color& color)
            : Position(position), Normal(normal), TexCoord(texCoord), Color(color) {}

        Vector3 Position  = { 0, 0, 0 };                // (x, y, z) in 3D space
        Vector3 Normal    = { 0, 0, 0 };                // (nx, ny, nz) for lighting
        Vector2I TexCoord = { 0, 0 };                   // (u, v) for texture mapping
        Color Color       = { 1.0f, 1.0f, 1.0f, 1.0f }; // (r, g, b, a) for color
    };

    static VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices)
    {
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices * 12);
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

            const auto& textCoord = vertex.TexCoord;
            meshPoints[positionToPlace + 10] = (float)textCoord.X;
            meshPoints[positionToPlace + 11] = (float)textCoord.Y;

            positionToPlace += 12;
        }

        const Tbx::BufferLayout& bufferLayout =
        {
            { Tbx::ShaderDataType::Float3, "position" },
            { Tbx::ShaderDataType::Float4, "color" },
            { Tbx::ShaderDataType::Float3, "normal" },
            { Tbx::ShaderDataType::Float2, "textureCoord" },
        };

        return VertexBuffer(meshPoints, bufferLayout);
    }
}
