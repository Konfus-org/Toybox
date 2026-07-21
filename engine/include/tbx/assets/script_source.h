#pragma once
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Script source loaded from a script file (name = the file name) — an ordinary
    /// asset like any other; load it with assets.load<ScriptSource>(...).
    struct ScriptSource
    {
        std::string name = {};
        std::string source = {};
    };
}
