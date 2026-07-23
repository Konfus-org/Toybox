#pragma once
#include "tbx/api.h"
#include "tbx/utils/uuid.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Base of every asset type: the identity an asset was loaded under rides with
    /// its data — a loaded asset always knows its own handle (id + tracked relative path).
    /// The asset system stamps both when it decodes; hand-made instances leave them empty.
    struct TBX_DLL_EXPORT Asset
    {
        Uuid id = {};
        std::string path = {};
    };
}
