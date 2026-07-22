#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Shader source asset — plain text (Format::TEXT), compiled by the gpu backend
    /// on use.
    struct TBX_API ShaderSource : Asset
    {
        std::string text = {};
    };
}
