#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/utils/color.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/texture.h"
#include "tbx/reflection/attributes.h"
#include <utility>


namespace tbx
{
    /// @brief
    /// Purpose: The sky: an equirectangular texture rendered behind everything. One per
    /// sandbox (the first wins), usually on a dedicated environment toy.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT Sky : Block
    {
        AssetHandle<Texture> texture = {};
        Color tint = {};

        // Fluent setters — each returns *this for one-chain construction.
        Sky& set_texture(AssetHandle<Texture> value) { texture = std::move(value); return *this; }
        Sky& set_tint(Color value) { tint = value; return *this; }
    };
}
