#pragma once
#include "tbx/core/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Linear-space RGBA color, components in [0, 1] (engine-owned, backend-free).
    struct TBX_API Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };
}

namespace tbx::colors
{
    inline constexpr Color BLACK = {.r = 0.0f, .g = 0.0f, .b = 0.0f};
    inline constexpr Color BLUE = {.r = 0.2f, .g = 0.4f, .b = 1.0f};
    inline constexpr Color CYAN = {.r = 0.1f, .g = 0.9f, .b = 0.9f};
    inline constexpr Color GRAY = {.r = 0.5f, .g = 0.5f, .b = 0.5f};
    inline constexpr Color GREEN = {.r = 0.2f, .g = 0.9f, .b = 0.3f};
    inline constexpr Color MAGENTA = {.r = 0.9f, .g = 0.2f, .b = 0.9f};
    inline constexpr Color ORANGE = {.r = 1.0f, .g = 0.6f, .b = 0.1f};
    inline constexpr Color RED = {.r = 1.0f, .g = 0.2f, .b = 0.2f};
    inline constexpr Color WHITE = {.r = 1.0f, .g = 1.0f, .b = 1.0f};
    inline constexpr Color YELLOW = {.r = 1.0f, .g = 0.9f, .b = 0.2f};
}
