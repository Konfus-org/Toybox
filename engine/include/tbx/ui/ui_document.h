#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/ui/font.h"
#include "tbx/utils/api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: UI document asset (.rml) — plain RML/RCSS text handed to tbx::ui.
    struct TBX_API UiDocument : Asset
    {
        std::string source = {};
        AssetHandle<Font> font = {};
    };

    /// @brief
    /// Purpose: Loads a UiDocument from disk (implementation lives next to the type).
    template <>
    TBX_API Result<UiDocument> load<UiDocument>(const std::filesystem::path& path);

}
