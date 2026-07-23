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
    /// Purpose: A font face as an ordinary asset (.ttf/.otf files): the raw file bytes.
    /// Decoding is the ui backend's business — set_font registers a face under a family
    /// name that documents reference in their styles.
    struct TBX_SERIALIZABLE(SerializerFormat::CUSTOM, reader=&deserialize_font) TBX_DLL_EXPORT Font : Asset
    {
        std::vector<std::byte> data = {};
    };

    /// @brief
    /// Purpose: Font's registered reader (the raw file bytes) — call it through
    /// deserialize<Font>(path).
    TBX_DLL_EXPORT Result<Font> deserialize_font(const std::filesystem::path& path);
}
