#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/ui/font.h"
#include "tbx/utils/api.h"
#include <string>

namespace tbx::ui
{
    /// @brief
    /// Purpose: UI document asset (.rml) — plain RML/RCSS text handed to tbx::ui.
    struct TBX_API UiDocument : assets::Asset
    {
        std::string source = {};
        assets::AssetHandle<Font> font = {};
    };

}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a UiDocument from disk (implementation lives next to the type).
    template <>
    TBX_API Result<ui::UiDocument> load<ui::UiDocument>(const std::filesystem::path& path);
}
