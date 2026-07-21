#pragma once
#include <cstddef>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded RGBA8 image asset.
    struct Texture
    {
        int width = 0;
        int height = 0;
        std::vector<std::byte> pixels = {};
    };
}
