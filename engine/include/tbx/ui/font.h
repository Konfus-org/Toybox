#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <cstddef>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: A font face as an ordinary asset (.ttf/.otf files): the raw file bytes.
    /// Decoding is the ui backend's business — ui::set_font registers a face under a family
    /// name that documents reference in their styles.
    struct TBX_API Font : Asset
    {
        std::vector<std::byte> data = {};
    };

    /// @brief
    /// Purpose: Loads a font file's bytes.
    template <>
    TBX_API Result<Font> load<Font>(const std::filesystem::path& path);
}
