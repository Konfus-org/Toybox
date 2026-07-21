#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/material.h"
#include "tbx/assets/model.h"
#include "tbx/assets/texture.h"
#include "tbx/utils/color.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Makes a toy visible: an imported model when the handle is set, otherwise a
    /// builtin primitive by name (tbx::builtin), surfaced by its material (falling back to a
    /// bare texture), always tinted.
    struct TBX_API Renderer
    {
        AssetHandle<Material> material = {};
        AssetHandle<Model> model = {};
        AssetHandle<Texture> texture = {};
        std::string mesh = "cube";
        Color tint = {};
    };
}
