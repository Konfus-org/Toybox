#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"
#include "tbx/reflection/attributes.h"

namespace tbx
{
    /// @brief
    /// Purpose: The ears: sounds spatialize relative to the first enabled listener's Transform.
    struct TBX_SERIALIZABLE() TBX_EXPOSED_TO_SCRIPTING TBX_DLL_EXPORT AudioListener : Block
    {
        float volume = 1.0f;

        // Fluent setter — returns *this for one-chain construction.
        AudioListener& set_volume(float value) { volume = value; return *this; }
    };
}
