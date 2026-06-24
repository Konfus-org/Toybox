#pragma once
#include "tbx/systems/assets/serialization.h"
#include "tbx/types/assets/asset.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.generated.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/matrices.h"

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
    [[serializable]];
    [[version(1U)]];
    struct TBX_API Model : Asset
    {
        Model();
        explicit Model(Mesh mesh);
        Model(Mesh mesh, Material material);

        // Geometry, materials, and hierarchy are populated by the model loader from the source asset
        // rather than persisted in the .model file (a version-only asset), so none are serialized.
        [[do_not_serialize]]
        std::vector<Mesh> meshes = {};
        [[do_not_serialize]]
        std::vector<Material> materials = {};
        [[do_not_serialize]]
        std::vector<ModelPart> parts = {};
    };

    /// @brief
    /// Purpose: Provides model-specific read parameters for serialized model assets.
    /// @details
    /// Ownership: Value type settings owned by the caller.
    /// Thread Safety: Safe to copy between threads.
    struct ModelLoadParameters
    {
        bool operator==(const ModelLoadParameters& other) const = default;
    };

    ModelLoadParameters load_parameters_of(const Model&);
}
