#pragma once
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Shader source asset — plain text, compiled by the gfx backend on use.
    struct ShaderSource
    {
        std::string text = {};
    };
}
