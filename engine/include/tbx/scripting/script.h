#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/ecs/block.h"
#include "tbx/scripting/script_source.h"
#include "tbx/utils/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// defines start(toy), update(toy, delta_time), and fixed_update(toy, delta_time).
    struct TBX_API Script : Block
    {
        assets::AssetHandle<ScriptSource> source = {};
    };
}
