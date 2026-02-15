#pragma once
#include "tbx/common/handle.h"
#include "tbx/common/int.h"
#include "tbx/graphics/vertex.h"
#include "tbx/tbx_api.h"
#include <memory>
#include <utility>
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

    /// <summary>
    /// Purpose: Identifies a static, asset-backed model to render for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores a non-owning model handle reference.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    /// </remarks>
    struct TBX_API StaticMesh
    {
        /// <summary>
        /// Purpose: Model asset handle that provides mesh geometry (and optional part materials).
        /// </summary>
        /// <remarks>
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        /// </remarks>
        Handle handle = {};
    };

    /// <summary>
    /// Purpose: Identifies runtime-owned mesh geometry to render for an entity.
    /// </summary>
    /// <remarks>
    /// Ownership: Holds a shared pointer to mesh data owned by callers or producer systems.
    /// Thread Safety: Mesh content mutation must be synchronized externally; the shared pointer
    /// itself is safe to copy between threads.
    /// </remarks>
    struct TBX_API DynamicMesh
    {
        DynamicMesh() = default;
        DynamicMesh(Mesh mesh)
            : data(std::make_shared<Mesh>(std::move(mesh)))
        {
        }
        DynamicMesh(std::shared_ptr<Mesh> mesh_data)
            : data(std::move(mesh_data))
        {
        }

        /// <summary>
        /// Purpose: Mesh data to render.
        /// </summary>
        /// <remarks>
        /// Ownership: Shared ownership of the mesh data via std::shared_ptr.
        /// Thread Safety: Safe to copy; synchronize mutation of the pointed-to Mesh externally.
        /// </remarks>
        std::shared_ptr<Mesh> data = {};
    };
}
