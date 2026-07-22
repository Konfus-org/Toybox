#pragma once
#include "tbx/ecs/block.h"
#include "tbx/utils/api.h"
#include "tbx/utils/color.h"

namespace tbx::gfx
{
    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct TBX_API DirectionalLight : ecs::Block
    {
        Color color = {};
        float intensity = 1.0f;
    };
}
