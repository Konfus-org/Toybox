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
        Model() = default;

        Mesh mesh = {};
        Material material = {};
        Uuid id = Uuid::generate();
    };
}
