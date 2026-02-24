#pragma once
#include "tbx/tbx_api.h"
#include <format>
#include <string>

namespace tbx
{
    struct TBX_API Color
    {
        // Default constructor, initializes the color to black (0, 0, 0, 1)
        Color() = default;

        // Constructor that initializes the color with the given RGBA values.
        // Given values are expected to be in the range of 0-1.
        Color(float r_value, float g_value, float b_value, float a_value)
            : r(r_value)
            , g(g_value)
            , b(b_value)
            , a(a_value)
        {
        }

        std::string to_string() const
        {
            return std::format("R: {}, G: {}, B: {}, A: {}", r, g, b, a);
        }

        // Amount of red in the color (0-1)
        float r = 0;
        // Amount of green in the color (0-1)
        float g = 0;
        // Amount of blue in the color (0-1)
        float b = 0;
        // Alpha/Transparency value (0-1)
        float a = 1;

        static const Color WHITE;
        static const Color BLACK;
        static const Color RED;
        static const Color GREEN;
        static const Color BLUE;
        static const Color YELLOW;
        static const Color CYAN;
        static const Color MAGENTA;
        static const Color GREY;
        static const Color LIGHT_GREY;
        static const Color DARK_GREY;
    };
}
