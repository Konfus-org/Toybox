#pragma once
#include "tbx/api.h"
#include "tbx/ecs/block.h"

namespace tbx::audio
{
    /// @brief
    /// Purpose: The ears: sounds spatialize relative to the first enabled listener's Transform.
    struct TBX_API Listener : ecs::Block
    {
        float volume = 1.0f;

        // Fluent setter — returns *this for one-chain construction.
        Listener& set_volume(float value) { volume = value; return *this; }
    };
}
