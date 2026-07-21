#pragma once
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded triangle mesh asset (assimp-imported, meshes merged, triangulated):
    /// interleaved position(3) + normal(3) + uv(2) floats.
    struct Model
    {
        std::vector<float> vertices = {};
    };
}
