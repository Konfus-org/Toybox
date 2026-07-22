#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Script source loaded from a script file (name = the file name) — an ordinary
    /// asset like any other; load it through the assets handle API or
    /// deserialize<ScriptSource>(path).
    struct TBX_API ScriptSource : Asset
    {
        std::string name = {};
        std::string source = {};
    };

    /// @brief
    /// Purpose: ScriptSource's registered reader — the file's text plus its file name (which
    /// is why this is a custom reader and not Format::TEXT).
    TBX_API Result<ScriptSource> deserialize_script_source(const std::filesystem::path& path);
}
