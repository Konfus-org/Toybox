#pragma once
#include "tbx/api.h"
#include "tbx/utils/color.h"
#include "tbx/ecs/block.h"

namespace tbx
{
    /// @brief
    /// Purpose: The sun: colored directional light casting shadows; direction is the owning
    /// toy's Transform forward (-Z).
    struct TBX_API DirectionalLight : Block
    {
        Color color = {};
        float intensity = 1.0f;

        // Fluent setters — each returns *this for one-chain construction.
        DirectionalLight& set_color(Color value) { color = value; return *this; }
        DirectionalLight& set_intensity(float value) { intensity = value; return *this; }
    };
}
