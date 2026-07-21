#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/texture.h"
#include "tbx/utils/color.h"

namespace tbx
{
    /// @brief
    /// Purpose: The sky: an equirectangular texture rendered behind everything. One per
    /// sandbox (the first wins), usually on a dedicated environment toy.
    struct TBX_API Sky
    {
        AssetHandle<Texture> texture = {};
        Color tint = {};
    };
}
