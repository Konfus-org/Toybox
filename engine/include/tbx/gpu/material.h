#pragma once
#include "tbx/api.h"
#include "tbx/assets/asset.h"
#include "tbx/assets/handle.h"
#include "tbx/utils/color.h"
#include "tbx/gpu/shader_source.h"
#include "tbx/gpu/texture.h"
#include <utility>

namespace tbx::gpu
{
    /// @brief
    /// Purpose: PBR material asset (.mat): how a surface renders. Metallic-roughness workflow
    /// (albedo/normal/metallic-roughness maps + scalar factors + emissive), optionally shaded
    /// by custom vertex AND fragment stages (either falls back to the builtin pbr stage), plus
    /// a free-form uniforms bag applied through shader reflection so arbitrary shaders just
    /// work. The .mat file references shaders/textures by asset-relative path; decoding
    /// resolves them to handles.
    struct TBX_API Material : assets::Asset
    {
        assets::Handle<ShaderSource> vertex = {};
        assets::Handle<ShaderSource> fragment = {};
        assets::Handle<Texture> albedo_map = {};
        assets::Handle<Texture> normal_map = {};
        assets::Handle<Texture> metallic_map = {};
        assets::Handle<Texture> roughness_map = {};
        Color albedo = {};
        Color emissive = Color {.r = 0.0f, .g = 0.0f, .b = 0.0f};
        float metallic = 0.0f;
        float roughness = 0.8f;
        float uv_scale = 1.0f;

        // Fluent setters — each returns *this so a Material can be built in one chain, e.g.
        // Material{}.set_albedo(red).set_roughness(0.3f). Designated-initializer construction
        // still works; these are just an alternative.
        Material& set_vertex(assets::Handle<ShaderSource> value)
        {
            vertex = std::move(value);
            return *this;
        }
        Material& set_fragment(assets::Handle<ShaderSource> value)
        {
            fragment = std::move(value);
            return *this;
        }
        Material& set_albedo_map(assets::Handle<Texture> value)
        {
            albedo_map = std::move(value);
            return *this;
        }
        Material& set_normal_map(assets::Handle<Texture> value)
        {
            normal_map = std::move(value);
            return *this;
        }
        Material& set_metallic_map(assets::Handle<Texture> value)
        {
            metallic_map = std::move(value);
            return *this;
        }
        Material& set_roughness_map(assets::Handle<Texture> value)
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
