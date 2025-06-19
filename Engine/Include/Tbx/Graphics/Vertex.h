#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Graphics/Color.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    struct EXPORT Vertex
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
}
