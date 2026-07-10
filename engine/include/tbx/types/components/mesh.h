#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/components/mesh.generated.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/vertex.h"

namespace tbx
{
    using IndexBuffer = std::vector<uint32>;

    [[serializable]];
    struct TBX_API Mesh
    {
        // Defaults to a quad.
        Mesh();
        Mesh(VertexBuffer vert_buff, IndexBuffer index_buff);

        /// @brief
        /// Purpose: Returns the number of float components in each mesh vertex.
        /// @details
        /// Ownership: Reads mesh metadata and returns a value type.
        /// Thread Safety: Safe to call concurrently when the mesh is not being mutated.
        uint32 get_vertex_stride_float_count() const;

        VertexBuffer vertices = {};
        IndexBuffer indices = {};
        MeshBounds bounds = {};

        /// @brief Purpose: Provides a triangle mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh TRIANGLE;

        /// @brief Purpose: Provides a quad mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh QUAD;

        /// @brief Purpose: Provides a clip-space fullscreen quad mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh FULLSCREEN_QUAD;

        /// @brief Purpose: Provides a cube mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh CUBE;

        /// @brief Purpose: Provides a sphere mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh SPHERE;

        /// @brief Purpose: Provides a capsule mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh CAPSULE;

        /// @brief Purpose: Provides a half-sphere mesh.
        /// @details Ownership: References static mesh data owned by the module.
        /// Thread Safety: Safe to read concurrently.
        static const Mesh HALF_SPHERE;
    };
}
