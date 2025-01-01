#pragma once
#include "tbxAPI.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"

namespace Toybox
{
    struct TBX_API Vertex
    {
        Vertex() = default;

        explicit(false) Vertex(const Vector3& position) 
            : Position(position), Normal(0, 0, 0), TexCoord(0, 0) {}

        Vertex(const Vector3& position, const Vector3& normal, const Vector2& texCoord)
            : Position(position), Normal(normal), TexCoord(texCoord) {}

        Vector3 Position;  // (x, y, z) in 3D space
        Vector3 Normal;    // (nx, ny, nz) for lighting
        Vector2 TexCoord;  // (u, v) for texture mapping
    };

    static std::vector<float> FlattenVertexVector(const std::vector<Vertex>& vertices)
    {
        auto numberOfVertices = vertices.size();
        std::vector<float> meshPoints(numberOfVertices * 3);
        int positionToPlace = 0;
        for (const auto& vertex : vertices)
        {
            const auto& position = vertex.Position;
            meshPoints[positionToPlace] = position.X;
            meshPoints[positionToPlace + 1] = position.Y;
            meshPoints[positionToPlace + 2] = position.Z;
            positionToPlace += 3;
        }
        return meshPoints;
    }
}
