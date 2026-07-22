#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/utils/result.h"
#include <filesystem>
#include <string>

namespace tbx::scripts
{
    /// @brief
    /// Purpose: Script source loaded from a script file (name = the file name) — an ordinary
    /// asset like any other; load it through the assets handle API or
    /// serialization::deserialize<Source>(path).
    struct TBX_API Source : assets::Asset
    {
        std::string name = {};
        std::string source = {};
    };

    /// @brief
    /// Purpose: Source's registered reader — the file's text plus its file name (which
    /// is why this is a custom reader and not Format::TEXT).
    TBX_API Result<Source> deserialize_script_source(const std::filesystem::path& path);
}
