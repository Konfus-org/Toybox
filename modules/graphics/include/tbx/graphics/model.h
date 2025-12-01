#pragma once
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// A model is just a mesh and a material
    /// </summary>
    struct TBX_API Model
    {
        Mesh mesh;
        Material material;
        uuid id = uuid::generate();
    };

    /// <summary>
    /// References a shared model by identifier.
    /// </summary>
    struct TBX_API ModelInstance
    {
        uuid model_id = {};
        uuid instance_id = uuid::generate();
    };
}
