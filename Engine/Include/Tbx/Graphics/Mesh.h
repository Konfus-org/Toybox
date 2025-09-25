#pragma once
#include "Tbx/Graphics/Vertex.h"
#include "Tbx/Math/Int.h"
#include "Tbx/Ids/Uid.h"

namespace Tbx
{
    struct TBX_EXPORT Mesh
    {
        static Mesh Quad;
        static Mesh Triangle;

        VertexBuffer VertexBuffer = {};
        std::vector<uint32> IndexBuffer = {};
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

