#pragma once
#include "tbx/assets/load.h"
#include "tbx/reflect/json_walker.h"
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: A set of things: toys (with their blocks and stickers) plus references to
    /// other kits, recursively — one concept covering prefab, scene, level, and chunk. A kit
    /// is an ordinary asset (AssetHandle<Kit>, .kit files); the body is its parsed serialized
    /// form, and only the sandbox's instantiation code looks inside it.
    struct TBX_API Kit
    {
        Json body = {};
    };

    /// @brief
    /// Purpose: Loads a .kit file (validated JSON kit body).
    template <>
    TBX_API Result<Kit> load<Kit>(const std::filesystem::path& path);
}
