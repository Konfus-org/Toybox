#pragma once
#include "tbx/graphics/vertex.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"
#include <cstdint>
#include <vector>

namespace tbx
{
    using IndexBuffer = std::vector<std::uint32_t>;

    struct TBX_API Mesh
    {
        // Defaults to a quad.
        Mesh();
        Mesh(const VertexBuffer& vertBuff, const IndexBuffer& indexBuff);

        VertexBuffer vertices = {};
        IndexBuffer indices = {};
        Uuid id = Uuid::generate();
    };

    // Essentially a pointer to a mesh.
    struct TBX_API MeshInstance
    {
        Uuid mesh_id = {};
        Uuid instance_id = Uuid::generate();
    };

    Mesh make_triangle();
    Mesh make_quad();
}

