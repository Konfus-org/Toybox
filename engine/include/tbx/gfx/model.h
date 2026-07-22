#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <vector>

namespace tbx::gfx
{
    /// @brief
    /// Purpose: Decoded triangle mesh asset (assimp-imported, meshes merged, triangulated):
    /// interleaved position(3) + normal(3) + uv(2) floats.
    struct TBX_API Model : assets::Asset
    {
        std::vector<float> vertices = {};
    };

}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a Model from disk (implementation lives next to the type).
    template <>
    TBX_API Result<gfx::Model> load<gfx::Model>(const std::filesystem::path& path);
}
