#pragma once
#include "tbx/api.h"
#include "tbx/utils/uuid.h"
#include <string>

namespace tbx::assets
{
    /// @brief
    /// Purpose: Base of every asset type: the identity an asset was loaded under rides with
    /// its data — a loaded asset always knows its own handle (id + tracked relative path).
    /// The asset system stamps both when it decodes; hand-made instances leave them empty.
    struct TBX_API Asset
    {
        Uuid id = {};
        std::string path = {};
    };
}

namespace tbx
{
    // Spelled at the tbx level like Handle — deriving `: Asset` reads clean everywhere.
    using assets::Asset;
}
