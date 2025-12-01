#pragma once
#include "tbx/graphics/vertex.h"
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
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

        VertexBuffer vertices = {};
        IndexBuffer indices = {};
        uuid id = uuid::generate();
    };

    /// <summary>
    /// Essentially a pointer to a mesh.
    /// </summary>
    struct TBX_API MeshInstance
    {
        uuid mesh_id = {};
        uuid instance_id = uuid::generate();
    };

    Mesh make_triangle();
    Mesh make_quad();
}

