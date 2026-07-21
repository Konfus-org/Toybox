#pragma once
#include "tbx/utils/api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Shader source asset — plain text, compiled by the gfx backend on use.
    struct TBX_API ShaderSource
    {
        std::string text = {};
    };
}
