#pragma once
#include "tbx/utils/api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: UI document asset (.rml) — plain RML/RCSS text handed to tbx::ui.
    struct TBX_API UiDocument
    {
        std::string text = {};
    };
}
