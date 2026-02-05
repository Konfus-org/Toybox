#pragma once
#include "tbx/common/int.h"
#include "tbx/graphics/vertex.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    using IndexBuffer = std::vector<uint32>;

    struct TBX_API Mesh
    {
        // Defaults to a quad.
        Mesh();
        Mesh(VertexBuffer vert_buff, IndexBuffer index_buff);

        VertexBuffer vertices = {};
        IndexBuffer indices = {};
    };

    TBX_API Mesh make_triangle();
    TBX_API Mesh make_quad();

    /// <summary>Purpose: Provides a triangle mesh.</summary>
    /// <remarks>Ownership: Returns a reference to the default triangle mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline Mesh triangle = make_triangle();

    /// <summary>Purpose: Provides a quad mesh.</summary>
    /// <remarks>Ownership: Returns a reference to the default quad mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline Mesh quad = make_quad();
}
