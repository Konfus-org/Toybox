#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/load.h"
#include "tbx/ecs/kit.h"
#include "tbx/math/math.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: How a box-level kit entry loads — set ONLY at the box level; nested kit
    /// references always load with whatever pulls them in.
    enum class KitMode : uint8
    {
        ALWAYS,
        STREAMED
    };

    /// @brief
    /// Purpose: One kit inside a box: which kit, how it loads, and where it sits.
    struct TBX_API BoxEntry
    {
        assets::AssetHandle<Kit> kit = {};
        KitMode mode = KitMode::ALWAYS;
        Vec3 position = Vec3(0.0f, 0.0f, 0.0f);
    };

    /// @brief
    /// Purpose: A box of kits (.box files): the kit entries a sandbox opens with. ALWAYS
    /// entries load at open; STREAMED entries load/unload by distance.
    struct TBX_API Box : assets::Asset
    {
        std::vector<BoxEntry> kits = {};
    };
}
