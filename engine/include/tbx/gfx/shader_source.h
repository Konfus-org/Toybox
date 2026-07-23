#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/reflection/attributes.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Shader source asset — plain text (Format::TEXT), compiled by the gpu backend
    /// on use.
    struct TBX_SERIALIZABLE(SerializerFormat::TEXT) TBX_DLL_EXPORT ShaderSource : Asset
    {
        std::string text = {};
    };
}
