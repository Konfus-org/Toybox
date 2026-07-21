#pragma once
#include "tbx/utils/api.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded triangle mesh asset (assimp-imported, meshes merged, triangulated):
    /// interleaved position(3) + normal(3) + uv(2) floats.
    struct TBX_API Model
    {
        std::vector<float> vertices = {};
    };
}
