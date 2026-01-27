#pragma once
#include "tbx/common/int.h"
#include "tbx/common/uuid.h"
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

    /// <summary>Purpose: Retrieves the default triangle mesh instance.</summary>
    /// <remarks>Ownership: Returns a reference to the mesh owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const Mesh& get_triangle_mesh();

    /// <summary>Purpose: Retrieves the default quad mesh instance.</summary>
    /// <remarks>Ownership: Returns a reference to the mesh owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const Mesh& get_quad_mesh();

    TBX_API Mesh make_triangle();
    TBX_API Mesh make_quad();

    /// <summary>Purpose: Provides the default triangle mesh instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default triangle mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Mesh& triangle = get_triangle_mesh();

    /// <summary>Purpose: Provides the default quad mesh instance.</summary>
    /// <remarks>Ownership: Returns a reference to the default quad mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.</remarks>
    inline const Mesh& quad = get_quad_mesh();
}

