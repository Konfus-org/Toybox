#pragma once
#include "tbx/common/uuid.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // A model is just a mesh and a material
    struct TBX_API Model
    {
        /// <summary>Creates an empty model.</summary>
        /// <remarks>Purpose: Initializes the model with default mesh, material, and id.
        /// Ownership: Owns the mesh/material data by value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        Model();

        /// <summary>Copies a model instance.</summary>
        /// <remarks>Purpose: Duplicates mesh, material, and id data.
        /// Ownership: New instance copies mesh/material data by value.
        /// Thread Safety: Safe to copy on any thread.</remarks>
        Model(const Model& other);

        /// <summary>Moves a model instance.</summary>
        /// <remarks>Purpose: Transfers mesh/material data into a new model.
        /// Ownership: Ownership moves to the destination instance.
        /// Thread Safety: Safe to move on any thread.</remarks>
        Model(Model&& other) noexcept;

        /// <summary>Assigns by copying another model.</summary>
        /// <remarks>Purpose: Replaces contents with a copied model.
        /// Ownership: Destination copies mesh/material data by value.
        /// Thread Safety: Safe to copy-assign on any thread.</remarks>
        Model& operator=(const Model& other);

        /// <summary>Assigns by moving another model.</summary>
        /// <remarks>Purpose: Transfers contents from the source model.
        /// Ownership: Destination takes ownership of moved data.
        /// Thread Safety: Safe to move-assign on any thread.</remarks>
        Model& operator=(Model&& other) noexcept;

        /// <summary>Destroys the model instance.</summary>
        /// <remarks>Purpose: Releases owned mesh/material data.
        /// Ownership: Releases mesh/material data on destruction.
        /// Thread Safety: Safe to destroy on any thread.</remarks>
        ~Model() noexcept;

        Mesh mesh = {};
        Material material = {};
        Uuid id = Uuid::generate();
    };
}
