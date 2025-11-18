#pragma once
#include "tbx/graphics/vertex.h"
#include "tbx/std/int.h"
#include "tbx/std/uuid.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    using IndexBuffer = std::vector<uint32>;

    struct TBX_API Mesh
    {
        /// <summary>
        /// Defaults to a quad.
        /// </summary>
        Mesh();
        Mesh(const VertexBuffer& vertBuff, const IndexBuffer& indexBuff);

        static Mesh quad;
        static Mesh triangle;

        VertexBuffer vertices = {};
        IndexBuffer indices = {};
        Uuid id = Uuid::generate();
    };

    /// <summary>
    /// Essentially a pointer to a mesh.
    /// </summary>
    struct TBX_API MeshInstance
    {
        Uuid mesh_id = {};
        Uuid instance_id = Uuid::generate();
    };

    Mesh make_triangle();
    Mesh make_quad();
}

