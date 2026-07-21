#pragma once
#include "tbx/assets/asset_handle.h"
#include "tbx/assets/shader_source.h"
#include "tbx/assets/texture.h"
#include "tbx/core/color.h"
#include "tbx/serialization/json.h"

namespace tbx
{
    /// @brief
    /// Purpose: Material asset (.mat): how a surface renders — an optional custom fragment
    /// shader (paired with the builtin lit vertex stage), an albedo texture, a tint, and a
    /// free-form uniforms bag applied through shader reflection so arbitrary shaders just
    /// work. The .mat file references shader/texture by asset-relative path; decoding resolves
    /// them to handles.
    struct Material
    {
        AssetHandle<ShaderSource> fragment = {};
        AssetHandle<Texture> texture = {};
        Color tint = {};
        Json uniforms = {};
    };
}
