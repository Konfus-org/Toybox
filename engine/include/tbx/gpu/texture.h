#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/utils/result.h"
#include <cstddef>
#include <filesystem>
#include <vector>

namespace tbx::gpu
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
    /// Purpose: Texture's registered reader (stb: PNG/JPG/BMP/...) — call it through
    /// serialization::deserialize<Texture>(path).
    TBX_API Result<Texture> deserialize_texture(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Texture's registered writer — a 32-bit BMP (screenshots and tooling; the
    /// reader takes it back). Call it through serialization::serialize(texture, path).
    TBX_API Result<void> serialize_texture(const Texture& texture, const std::filesystem::path& path);
}
