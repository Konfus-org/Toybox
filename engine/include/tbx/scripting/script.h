#pragma once
#include "tbx/api.h"
#include "tbx/assets/handle.h"
#include "tbx/ecs/block.h"
#include "tbx/reflection/attributes.h"
#include "tbx/scripting/source.h"

namespace tbx
{
    /// @brief
    /// Purpose: The block that makes a toy scripted: names a loaded script source. A script
    /// defines start(toy), update(toy, delta_time), fixed_update(toy, delta_time), and
    /// cleanup(toy) — the last called once before the instance is unloaded.
    struct TBX_SERIALIZABLE() TBX_DLL_EXPORT Script : Block
    {
        AssetHandle<ScriptSource> source = {};
    };
}
