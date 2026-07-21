#pragma once

namespace tbx
{
    /// @brief
    /// Purpose: Linear-space RGBA color, components in [0, 1] (engine-owned, backend-free).
    struct Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };
}
