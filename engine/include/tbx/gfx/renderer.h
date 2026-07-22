#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/material.h"
#include "tbx/gfx/model.h"
#include "tbx/utils/api.h"

namespace tbx::gfx
{
    /// @brief
    /// Purpose: Makes a toy visible: a model surfaced by a material. Builtin primitives are
    /// reserved model handles (tbx::builtin::CUBE/PLANE/SPHERE; unset renders the cube); an
    /// unset material renders the builtin white PBR surface.
    struct TBX_API Renderer : ecs::Block
    {
        assets::AssetHandle<Material> material = {};
        assets::AssetHandle<Model> model = {};
    };
}
