#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/assets/handle.h"
#include "tbx/ui/font.h"
#include <string>

namespace tbx::ui
{
    /// @brief
    /// Purpose: UI document asset (.rml) — plain RML/RCSS text (Format::TEXT) handed to
    /// tbx::ui.
    struct TBX_API Document : assets::Asset
    {
        std::string text = {};
        assets::Handle<Font> font = {};
    };
}
