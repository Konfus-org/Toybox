#pragma once
#include "tbx/utils/api.h"
#include "tbx/assets/asset_handle.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/texture.h"
#include "tbx/utils/color.h"
#include "tbx/serialization/json.h"

namespace tbx
{
    /// @brief
    /// Purpose: PBR material asset (.mat): how a surface renders. Metallic-roughness workflow
    /// (albedo/normal/metallic-roughness maps + scalar factors + emissive), optionally shaded
    /// by custom vertex AND fragment stages (either falls back to the builtin pbr stage), plus
    /// a free-form uniforms bag applied through shader reflection so arbitrary shaders just
    /// work. The .mat file references shaders/textures by asset-relative path; decoding
    /// resolves them to handles.
    struct TBX_API Material
    {
        AssetHandle<ShaderSource> vertex = {};
        AssetHandle<ShaderSource> fragment = {};
        AssetHandle<Texture> albedo_map = {};
        AssetHandle<Texture> normal_map = {};
        AssetHandle<Texture> metallic_roughness_map = {}; // glTF layout: g=roughness, b=metallic
        Color albedo = {};
        float metallic = 0.0f;
        float roughness = 0.8f;
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        Json uniforms = {};
    };
}
