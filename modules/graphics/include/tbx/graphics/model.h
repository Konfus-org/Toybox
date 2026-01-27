#pragma once
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"
#include <memory>

namespace tbx
{
    // A model is just a mesh and a material
    struct TBX_API Model
    {
        /// <summary>Creates an empty model.</summary>
        /// <remarks>Purpose: Initializes the model with default mesh, material, and id.
        /// Ownership: Shares ownership of mesh/material data.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        Model();

        /// <summary>Copies a model instance.</summary>
        /// <remarks>Purpose: Duplicates mesh, material, and id data.
        /// Ownership: New instance shares ownership of mesh/material data.
        /// Thread Safety: Safe to copy on any thread.</remarks>
        Model(const Model& other);

        /// <summary>Moves a model instance.</summary>
        /// <remarks>Purpose: Transfers mesh/material data into a new model.
        /// Ownership: Ownership moves to the destination instance.
        /// Thread Safety: Safe to move on any thread.</remarks>
        Model(Model&& other) noexcept;

        /// <summary>Assigns by copying another model.</summary>
        /// <remarks>Purpose: Replaces contents with a copied model.
        /// Ownership: Destination shares ownership of mesh/material data after assignment.
        /// Thread Safety: Safe to copy-assign on any thread.</remarks>
        Model& operator=(const Model& other);

        /// <summary>Assigns by moving another model.</summary>
        /// <remarks>Purpose: Transfers contents from the source model.
        /// Ownership: Destination takes ownership of moved data.
        /// Thread Safety: Safe to move-assign on any thread.</remarks>
        Model& operator=(Model&& other) noexcept;

        /// <summary>Destroys the model instance.</summary>
        /// <remarks>Purpose: Releases owned mesh/material data.
        /// Ownership: Releases shared ownership of mesh/material data on destruction.
        /// Thread Safety: Safe to destroy on any thread.</remarks>
        ~Model() noexcept;

        std::shared_ptr<Mesh> mesh = get_quad_mesh();
        std::shared_ptr<Material> material = get_standard_material();
        Uuid id = Uuid::generate();
    };

    /// <summary>Purpose: Retrieves the shared default model instance.</summary>
    /// <remarks>Ownership: Returns a shared pointer owned by the module.
    /// Thread Safety: Safe to call concurrently after static initialization.</remarks>
    TBX_API const std::shared_ptr<Model>& get_default_model();

    /// <summary>Represents an instance that references shared model data.</summary>
    struct TBX_API ModelInstance
    {
        /// <summary>Purpose: References shared model data for rendering.</summary>
        /// <remarks>Ownership: Holds a shared reference to the model data.
        /// Thread Safety: Safe to copy between threads, but model data mutation
        /// requires external synchronization.</remarks>
        std::shared_ptr<Model> model = get_default_model();
        Uuid instance_id = Uuid::generate();
    };
}
