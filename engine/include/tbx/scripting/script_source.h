#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Script source loaded from a script file (name = the file name) — an ordinary
    /// asset like any other; load it with assets.load<ScriptSource>(...).
    struct TBX_API ScriptSource : assets::Asset
    {
        std::string name = {};
        std::string source = {};
    };

}

namespace tbx::assets
{
    /// @brief
    /// Purpose: Loads a ScriptSource from disk (implementation lives next to the type).
    template <>
    TBX_API Result<ScriptSource> load<ScriptSource>(const std::filesystem::path& path);
}
