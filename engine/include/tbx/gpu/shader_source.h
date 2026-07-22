#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include <string>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: Shader source asset — plain text (Format::TEXT), compiled by the gpu backend
    /// on use.
    struct TBX_API ShaderSource : assets::Asset
    {
        std::string text = {};
    };
}
