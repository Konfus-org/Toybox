#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Script source asset — plain text (Format::TEXT), compiled by the scripting backend
    /// on load. An ordinary asset like any other; `text` holds the whole file, `path` (from Asset)
    /// carries its file name for diagnostics.
    struct TBX_API ScriptSource : Asset
    {
        std::string text = {};
    };
}
