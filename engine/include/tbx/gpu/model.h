#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <vector>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: Decoded triangle mesh asset (assimp-imported, meshes merged, triangulated):
    /// interleaved position(3) + normal(3) + uv(2) floats.
    struct TBX_API Model : assets::Asset
    {
        std::vector<float> vertices = {};
    };

    /// @brief
    /// Purpose: Model's registered reader (assimp) — call it through
    /// serialization::deserialize<Model>(path).
    TBX_API Result<Model> deserialize_model(const std::filesystem::path& path);
}
