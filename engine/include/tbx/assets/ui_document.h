#pragma once
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: UI document asset (.rml) — plain RML/RCSS text handed to tbx::ui.
    struct UiDocument
    {
        std::string text = {};
    };
}
