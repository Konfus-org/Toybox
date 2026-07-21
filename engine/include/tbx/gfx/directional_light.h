#pragma once
#include "tbx/utils/api.h"
#include "tbx/utils/color.h"

namespace tbx
{
    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct TBX_API DirectionalLight
    {
        Color color = {};
        float intensity = 1.0f;
    };
}
