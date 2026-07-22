#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <cstddef>
#include <vector>

namespace tbx::gfx
{
    /// @brief
    /// Purpose: Decoded RGBA8 image asset.
    struct TBX_API Texture : assets::Asset
    {
        int width = 0;
        int height = 0;
        std::vector<std::byte> pixels = {};
    };

    /// @brief
    /// Purpose: Writes the texture as a 32-bit BMP — screenshots and tooling; load<gfx::Texture>
    /// reads it back.
    TBX_API Result<void> save(const Texture& texture, const std::filesystem::path& path);
}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a Texture from disk (implementation lives next to the type).
    template <>
    TBX_API Result<gfx::Texture> load<gfx::Texture>(const std::filesystem::path& path);
}
