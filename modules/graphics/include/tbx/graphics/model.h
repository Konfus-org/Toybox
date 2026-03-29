#pragma once
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/math/matrices.h"
#include "tbx/tbx_api.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Represents a node in a model hierarchy that references mesh and material slots.
    /// @details
    /// Ownership: Stores owned child indices and non-owning references via indices.
    /// Thread Safety: Safe to copy between threads.

    struct TBX_API ModelPart
    {
        /// @brief
        /// Purpose: Stores the transform relative to the parent part, or model root when the part
        /// has no parent.
        /// @details
        /// Ownership: Value-owned matrix data with no external lifetime dependency.
        /// Thread Safety: Safe to read and copy concurrently.

        Mat4 transform = Mat4(1.0f);
        uint32 mesh_index = 0U;
        uint32 material_index = 0U;
        std::vector<uint32> children = {};
    };

    /// @brief
    /// Purpose: Defines a model made up of multiple meshes, materials, and hierarchical parts.
    /// @details
    /// Ownership: Owns mesh, material, and part data by value.
    /// Thread Safety: Safe to construct on any thread.

    struct TBX_API Model
    {
        Model();
        Model(Mesh mesh);
        Model(Mesh mesh, Material material);

        std::vector<Mesh> meshes = {};
        std::vector<Material> materials = {};
        std::vector<ModelPart> parts = {};
    };
}
