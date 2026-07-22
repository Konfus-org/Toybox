#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/utils/result.h"
#include <cstddef>
#include <filesystem>
#include <vector>

namespace tbx::ui
{
    /// @brief
    /// Purpose: A font face as an ordinary asset (.ttf/.otf files): the raw file bytes.
    /// Decoding is the ui backend's business — ui::set_font registers a face under a family
    /// name that documents reference in their styles.
    struct TBX_API Font : assets::Asset
    {
        std::vector<std::byte> data = {};
    };

    /// @brief
    /// Purpose: Font's registered reader (the raw file bytes) — call it through
    /// serialization::deserialize<Font>(path).
    TBX_API Result<Font> deserialize_font(const std::filesystem::path& path);
}
