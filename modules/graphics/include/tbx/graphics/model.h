#pragma once
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/math/matrices.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// <summary>
    /// Purpose: Represents a node in a model hierarchy that references mesh and material slots.
    /// </summary>
    /// <remarks>
    /// Ownership: Stores owned child indices and non-owning references via indices.
    /// Thread Safety: Safe to copy between threads.
    /// </remarks>
    struct TBX_API ModelPart
    {
        Mat4 transform = Mat4(1.0f);
        uint32 mesh_index = 0U;
        uint32 material_index = 0U;
        std::vector<uint32> children = {};
    };

    /// <summary>
    /// Purpose: Defines a model made up of multiple meshes, materials, and hierarchical parts.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns mesh, material, and part data by value.
    /// Thread Safety: Safe to construct on any thread.
    /// </remarks>
    struct TBX_API Model
    {
        /// <summary>Creates a model with a default mesh, material, and part.</summary>
        /// <remarks>Purpose: Initializes the model with a single mesh/material pair.
        /// Ownership: Owns the mesh/material/part data by value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        Model();

        /// <summary>Creates a model with a single mesh and default material.</summary>
        /// <remarks>Purpose: Initializes the model with the provided mesh and default material.
        /// Ownership: Owns the mesh/material/part data by value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        Model(Mesh mesh);

        /// <summary>Creates a model with a single mesh and material.</summary>
        /// <remarks>Purpose: Initializes the model with the provided mesh and material.
        /// Ownership: Owns the mesh/material/part data by value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        Model(Mesh mesh, Material material);

        std::vector<Mesh> meshes = {};
        std::vector<Material> materials = {};
        std::vector<ModelPart> parts = {};
    };
}
