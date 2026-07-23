#pragma once
#include "tbx/api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Linear-space RGBA color, components in [0, 1] (engine-owned, backend-free).
    /// The named palette lives on the type (Color::WHITE()) so it reads flat at tbx level
    /// without a colors namespace.
    struct TBX_DLL_EXPORT Color
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        static constexpr Color BLACK() { return {.r = 0.0f, .g = 0.0f, .b = 0.0f}; }
        static constexpr Color BLUE() { return {.r = 0.2f, .g = 0.4f, .b = 1.0f}; }
        static constexpr Color CYAN() { return {.r = 0.1f, .g = 0.9f, .b = 0.9f}; }
        static constexpr Color GRAY() { return {.r = 0.5f, .g = 0.5f, .b = 0.5f}; }
        static constexpr Color GREEN() { return {.r = 0.2f, .g = 0.9f, .b = 0.3f}; }
        static constexpr Color MAGENTA() { return {.r = 0.9f, .g = 0.2f, .b = 0.9f}; }
        static constexpr Color ORANGE() { return {.r = 1.0f, .g = 0.6f, .b = 0.1f}; }
        static constexpr Color RED() { return {.r = 1.0f, .g = 0.2f, .b = 0.2f}; }
        static constexpr Color WHITE() { return {.r = 1.0f, .g = 1.0f, .b = 1.0f}; }
        static constexpr Color YELLOW() { return {.r = 1.0f, .g = 0.9f, .b = 0.2f}; }
    };
}
