#pragma once
#include "tbxAPI.h"
#include "Color.h"
#include "Buffers.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"

namespace Toybox
{
    struct TBX_API Vertex
    {
        Vertex() = default;
        explicit(false) Vertex(const Vector3& position) 
            : Position(position), Normal(0, 0, 0), TexCoord(0, 0) {}
        Vertex(const Vector3& position, const Color& color)
            : Position(position), Normal(0, 0, 0), TexCoord(0, 0), Color(color) {}
        Vertex(const Vector3& position, const Vector3& normal, const Vector2& texCoord)
            : Position(position), Normal(normal), TexCoord(texCoord) {}

        Vector3 Position;  // (x, y, z) in 3D space
        Vector3 Normal;    // (nx, ny, nz) for lighting
        Vector2 TexCoord;  // (u, v) for texture mapping
        Color Color;       // (r, g, b, a) for color
    };

    static VertexBuffer VertexVectorToBuffer(const std::vector<Vertex>& vertices)
    {
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices * 7);
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

            // TODO: Add normal and texture coordinates.... rn renderer only uses position and color

            positionToPlace += 7;
        }


        const Toybox::BufferLayout& bufferLayout =
        {
                { Toybox::ShaderDataType::Float3, "position" },
                { Toybox::ShaderDataType::Float4, "color" }
        };

        return VertexBuffer(meshPoints, bufferLayout);
    }
}
