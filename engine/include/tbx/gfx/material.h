#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/assets/handle.h"
#include "tbx/utils/color.h"
#include "tbx/gfx/shader_source.h"
#include "tbx/gfx/texture.h"
#include <utility>

namespace tbx
{
    /// @brief
    /// Purpose: PBR material asset (.mat): how a surface renders. Metallic-roughness workflow
    /// (albedo/normal/metallic-roughness maps + scalar factors + emissive), optionally shaded
    /// by custom vertex AND fragment stages (either falls back to the builtin pbr stage), plus
    /// a free-form uniforms bag applied through shader reflection so arbitrary shaders just
    /// work. The .mat file references shaders/textures by asset-relative path; decoding
    /// resolves them to handles.
    struct TBX_API Material : Asset
    {
        AssetHandle<ShaderSource> vertex = {};
        AssetHandle<ShaderSource> fragment = {};
        AssetHandle<Texture> albedo_map = {};
        AssetHandle<Texture> normal_map = {};
        AssetHandle<Texture> metallic_map = {};
        AssetHandle<Texture> roughness_map = {};
        Color albedo = {};
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        float metallic = 0.0f;
        float roughness = 0.8f;
        float uv_scale = 1.0f;

        // Fluent setters — each returns *this so a Material can be built in one chain, e.g.
        // Material{}.set_albedo(red).set_roughness(0.3f). Designated-initializer construction
        // still works; these are just an alternative.
        Material& set_vertex(AssetHandle<ShaderSource> value)
        {
            vertex = std::move(value);
            return *this;
        }
        Material& set_fragment(AssetHandle<ShaderSource> value)
        {
            fragment = std::move(value);
            return *this;
        }
        Material& set_albedo_map(AssetHandle<Texture> value)
        {
            albedo_map = std::move(value);
            return *this;
        }
        Material& set_normal_map(AssetHandle<Texture> value)
        {
            normal_map = std::move(value);
            return *this;
        }
        Material& set_metallic_map(AssetHandle<Texture> value)
        {
            metallic_map = std::move(value);
            return *this;
        }
        Material& set_roughness_map(AssetHandle<Texture> value)
        {
            roughness_map = std::move(value);
            return *this;
        }
        Material& set_albedo(Color value)
        {
            albedo = value;
            return *this;
        }
        Material& set_emissive(Color value)
        {
            emissive = value;
            return *this;
        }
        Material& set_metallic(float value)
        {
            metallic = value;
            return *this;
        }
        Material& set_roughness(float value)
        {
            roughness = value;
            return *this;
        }
        Material& set_uv_scale(float value)
        {
            uv_scale = value;
            return *this;
        }
    };
}
