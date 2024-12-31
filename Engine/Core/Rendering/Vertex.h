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
}
