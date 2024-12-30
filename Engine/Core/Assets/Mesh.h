#pragma once
#include "tbxpch.h"
#include "Asset.h"
#include "Math/Int.h"
#include "Rendering/Vertex.h"

namespace Toybox
{
    class Mesh : public Asset
    {
    public:
        TBX_API Mesh(const std::vector<Vertex>& vertices, const std::vector<uint>& indices)
            : Vertices(vertices), Indices(indices) {}

        TBX_API inline std::vector<Vertex> GetVertices() const { return Vertices; }
        TBX_API inline std::vector<uint> GetIndices() const { return Indices; }

    protected:
        TBX_API  bool LoadData(const std::filebuf& fileContents) override;

    private:
        std::vector<Vertex> Vertices;
        std::vector<uint> Indices;
    };
}

