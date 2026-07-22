#pragma once
#include "tbx/ecs/block.h"
#include "tbx/utils/api.h"

namespace tbx::audio
{
    /// @brief
    /// Purpose: The ears: sounds spatialize relative to the first enabled listener's Transform.
    struct TBX_API AudioListener : ecs::Block
    {
        float volume = 1.0f;
    };
}
