#pragma once
#include "tbx/assets/asset.h"
#include "tbx/assets/load.h"
#include "tbx/utils/api.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Shader source asset — plain text, compiled by the gfx backend on use.
    struct TBX_API ShaderSource : Asset
    {
        std::string text = {};
    };

    /// @brief
    /// Purpose: Loads a ShaderSource from disk (implementation lives next to the type).
    template <>
    TBX_API Result<ShaderSource> load<ShaderSource>(const std::filesystem::path& path);

}
