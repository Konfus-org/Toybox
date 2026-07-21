#pragma once
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
}
