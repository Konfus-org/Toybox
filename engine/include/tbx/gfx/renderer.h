#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/material.h"
#include "tbx/assets/model.h"

namespace tbx
{
    /// @brief
    /// Purpose: Makes a toy visible: a model surfaced by a material. Builtin primitives are
    /// reserved model handles (tbx::builtin::CUBE/PLANE/SPHERE; unset renders the cube); an
    /// unset material renders the builtin white PBR surface.
    struct TBX_API Renderer
    {
        AssetHandle<Material> material = {};
        AssetHandle<Model> model = {};
    };
}
