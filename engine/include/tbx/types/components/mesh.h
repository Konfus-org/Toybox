#pragma once
#include "tbx/systems/files/serialization.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/typedefs.h"
#include "tbx/types/vertex.h"
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
        /// @brief
        /// Purpose: Stores local-space mesh bounds used for visibility and lighting culling.
        /// @details
        /// Ownership: Value-owned by the mesh. Notes: Static meshes are populated during model
        /// load. Dynamic mesh producers are responsible for refreshing bounds after mutating
        /// vertices/indices.
        MeshBounds bounds = {};
    };

    TBX_API Mesh make_triangle();

    TBX_API Mesh make_quad();

    /// @brief Purpose: Creates a clip-space fullscreen quad mesh.
    /// @details Ownership: Returns a mesh value owned by the caller.
    /// Thread Safety: Safe to call concurrently.
    TBX_API Mesh make_fullscreen_quad();

    TBX_API Mesh make_cube();

    TBX_API Mesh make_sphere();

    TBX_API Mesh make_capsule();

    /// @brief Purpose: Creates a panoramic sky dome mesh.
    /// @details Ownership: Returns a mesh value owned by the caller.
    /// Thread Safety: Safe to call concurrently.
    TBX_API Mesh make_sky_dome();

    /// @brief
    /// Purpose: Returns the number of float components in each mesh vertex.
    /// @details
    /// Ownership: Reads mesh metadata and returns a value type.
    /// Thread Safety: Safe to call concurrently when the mesh is not being mutated.
    TBX_API uint32 get_vertex_stride_float_count(const Mesh& mesh);

    /// @brief Purpose: Provides a triangle mesh.
    /// @details Ownership: Returns a reference to the default triangle mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh triangle = make_triangle();

    /// @brief Purpose: Provides a quad mesh.
    /// @details Ownership: Returns a reference to the default quad mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh quad = make_quad();

    /// @brief Purpose: Provides a clip-space fullscreen quad mesh.
    /// @details Ownership: Returns a reference to the default fullscreen quad mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh fullscreen_quad = make_fullscreen_quad();

    /// @brief Purpose: Provides a cube mesh.
    /// @details Ownership: Returns a reference to the default cube mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh cube = make_cube();

    /// @brief Purpose: Provides a sphere mesh.
    /// @details Ownership: Returns a reference to the default sphere mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh sphere = make_sphere();

    /// @brief Purpose: Provides a capsule mesh.
    /// @details Ownership: Returns a reference to the default capsule mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh capsule = make_capsule();

    /// @brief Purpose: Provides a panoramic sky dome mesh.
    /// @details Ownership: Returns a reference to the default sky dome mesh owned
    /// by the module.
    /// Thread Safety: Safe to read concurrently.
    inline Mesh sky_dome = make_sky_dome();

    /// @brief
    /// Purpose: Identifies a static, asset-backed model to render for an entity.
    /// @details
    /// Ownership: Stores a non-owning model handle reference.
    /// Thread Safety: Safe to copy between threads; mutation requires external synchronization.
    struct TBX_API StaticMesh
    {
        /// @brief
        /// Purpose: Model asset handle that provides mesh geometry (and optional part materials).
        /// @details
        /// Ownership: Stores a non-owning handle reference.
        /// Thread Safety: Safe to read concurrently; synchronize mutation externally.
        Handle handle = {};
    };

    /// @brief
    /// Purpose: Identifies reusable runtime mesh geometry shared by many renderable entities.
    /// @details
    /// Ownership: Holds a shared pointer to mesh data owned by a producer system.
    /// Thread Safety: Mesh content mutation must be synchronized externally; the shared pointer
    /// itself is safe to copy between threads.
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

        /// @brief
        /// Purpose: Shared mesh data to render.
        /// @details
        /// Ownership: Shared ownership of the mesh data via std::shared_ptr.
        /// Thread Safety: Safe to copy; synchronize mutation of the pointed-to Mesh externally.
        std::shared_ptr<Mesh> data = {};
    };
}
