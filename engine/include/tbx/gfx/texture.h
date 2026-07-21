#pragma once
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <cstddef>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded RGBA8 image asset.
    struct TBX_API Texture
    {
        int width = 0;
        int height = 0;
        std::vector<std::byte> pixels = {};
    };

    /// @brief
    /// Purpose: Loads a Texture from disk (implementation lives next to the type).
    template <>
    TBX_API Result<Texture> load<Texture>(const std::filesystem::path& path);

}
