#pragma once
#include "Tbx/Graphics/Vertex.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    using IndexBuffer = std::vector<uint32>;

    struct TBX_EXPORT Mesh
    {
        /// <summary>
        /// Defaults to a quad.
        /// </summary>
        Mesh();
        Mesh(const VertexBuffer& vertBuff, const IndexBuffer& indexBuff);

        static Mesh Quad;
        static Mesh Triangle;

        VertexBuffer Vertices = {};
        IndexBuffer Indices = {};
        Uid Id = Uid::Generate();
    };

    /// <summary>
    /// Essentially a pointer to a mesh.
    /// </summary>
    struct TBX_EXPORT MeshInstance
    {
        Uid MeshId = Uid::Invalid;
        Uid InstanceId = Uid::Generate();
    };

    static Mesh MakeTriangle();
    static Mesh MakeQuad();
}

