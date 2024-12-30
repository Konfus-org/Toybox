#pragma once
#include "Color.h"
#include "Math/Vector2.h"
#include "Math/Vector3.h"

namespace Toybox
{
    struct TBX_API Vertex
    {
        Vector3 Position;  // (x, y, z) in 3D space
        Vector3 Normal;    // (nx, ny, nz) for lighting
        Vector2 TexCoord;  // (u, v) for texture mapping
    };
}