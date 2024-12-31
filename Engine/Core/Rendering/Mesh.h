#pragma once
#include "tbxpch.h"
#include "Math/Int.h"
#include "Vertex.h"

namespace Toybox
{
    struct Mesh
    {
    public:
        TBX_API Mesh() = default;
        TBX_API Mesh(const std::vector<Vertex>& vertices, const std::vector<uint>& indices)
            : Vertices(vertices), Indices(indices) {}

        TBX_API inline std::vector<Vertex> GetVertices() const { return Vertices; }
        TBX_API inline std::vector<uint> GetIndices() const { return Indices; }

    private:
        std::vector<Vertex> Vertices; // Mesh vertices (points, normals, texture coords)
        std::vector<uint> Indices;    // Order in which to draw vertices, can also be used for instancing / re-using vertices in a mesh
    };
}

