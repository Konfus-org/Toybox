#pragma once
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/common/uuid.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // A model is just a mesh and a material
    struct TBX_API Model
    {
        Mesh mesh;
        Material material;
        Uuid id = Uuid::generate();
    };

    // References a shared model by identifier.
    struct TBX_API ModelInstance
    {
        Uuid model_id = {};
        Uuid instance_id = Uuid::generate();
    };
}
