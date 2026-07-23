#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Script source asset — plain text (Format::TEXT), compiled by the scripting backend
    /// on load. An ordinary asset like any other; `text` holds the whole file, `path` (from Asset)
    /// carries its file name for diagnostics.
    struct TBX_SERIALIZABLE(SerializerFormat::TEXT) TBX_DLL_EXPORT ScriptSource : Asset
    {
        std::string text = {};
    };
}
