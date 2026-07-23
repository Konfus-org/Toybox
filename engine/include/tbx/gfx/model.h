#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded triangle mesh asset (assimp-imported, meshes merged, triangulated):
    /// interleaved position(3) + normal(3) + uv(2) floats.
    struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&deserialize_model) TBX_DLL_EXPORT Model : Asset
    {
        std::vector<float> vertices = {};
    };

    /// @brief
    /// Purpose: Model's registered reader (assimp) — call it through
    /// deserialize<Model>(path).
    TBX_DLL_EXPORT Result<Model> deserialize_model(const std::filesystem::path& path);
}
