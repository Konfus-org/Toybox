#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/scripting/source.h"


namespace tbx::scripts
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// defines start(toy), update(toy, delta_time), and fixed_update(toy, delta_time).
    struct TBX_API Script : Block
    {
        assets::Handle<Source> source = {};
    };
}
