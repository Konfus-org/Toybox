#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include "tbx/utils/result.h"
#include <cstddef>
#include <filesystem>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: Decoded RGBA8 image asset.
    struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&deserialize_texture, writer=&serialize_texture) TBX_DLL_EXPORT Texture : Asset
    {
        int width = 0;
        int height = 0;
        std::vector<std::byte> pixels = {};
    };

    /// @brief
    /// Purpose: Texture's registered reader (stb: PNG/JPG/BMP/...) — call it through
    /// deserialize<Texture>(path).
    TBX_DLL_EXPORT Result<Texture> deserialize_texture(const std::filesystem::path& path);

    /// @brief
    /// Purpose: Texture's registered writer — a 32-bit BMP (screenshots and tooling; the
    /// reader takes it back). Call it through serialize(texture, path).
    TBX_DLL_EXPORT Result<void> serialize_texture(
        const Texture& texture,
        const std::filesystem::path& path);
}
