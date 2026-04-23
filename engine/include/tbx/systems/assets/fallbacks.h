#pragma once
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/audio/audio_clip.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/graphics/texture.h"
#include <cstddef>
#include <memory>
#include <vector>


namespace tbx
{
    template <typename TAsset>
    std::shared_ptr<TAsset> make_fallback_asset(const AssetLoadParameters<TAsset>& parameters = {});

    inline std::shared_ptr<AudioClip> make_fallback_audio_clip()
    {
        return std::make_shared<AudioClip>();
    }

    inline std::shared_ptr<Material> make_fallback_material()
    {
        auto material = Material();
        material.program.vertex = PbrVertexShader::HANDLE;
        material.program.fragment = PbrFragmentShader::HANDLE;
        material.parameters.set("color", Color(1.0f, 0.0f, 1.0f, 1.0f));
        material.parameters.set("diffuse_strength", 1.0f);
        material.parameters.set("normal_strength", 1.0f);
        material.parameters.set("specular_strength", 0.5f);
        material.parameters.set("shininess_strength", 32.0f);
        material.parameters.set("emissive", Color(1.0f, 0.0f, 1.0f, 1.0f));
        material.parameters.set("emissive_strength", 1.0f);
        material.parameters.set("alpha_cutoff", 0.1f);
        material.parameters.set("transparency_amount", 0.0f);
        material.parameters.set("exposure", 1.0f);
        material.config = MaterialConfig {
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_depth_prepass_enabled = false,
            .depth_function = MaterialDepthFunction::Less,
            .blend_mode = MaterialBlendMode::Opaque,
        };
        return std::make_shared<Material>(std::move(material));
    }

    inline std::shared_ptr<Texture> make_fallback_texture(
        const TextureLoadParameters& parameters = {})
    {
        const Texture& texture = parameters.texture;
        auto pixels = std::vector<Pixel>();
        if (texture.format == TextureFormat::RGB)
            pixels = {255, 0, 255};
        else
            pixels = {255, 0, 255, 255};

        return std::make_shared<Texture>(
            Size(1, 1),
            texture.wrap,
            texture.filter,
            texture.format,
            texture.mipmaps,
            texture.compression,
            pixels);
    }

    inline std::shared_ptr<Shader> make_fallback_shader()
    {
        auto vertex_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) in vec3 a_position;\n"
            "layout(location = 8) in mat4 a_model;\n"
            "layout(location = 12) in uint a_instance_id;\n"
            "uniform mat4 u_view_proj = mat4(1.0);\n"
            "void main()\n"
            "{\n"
            "    gl_Position = u_view_proj * (a_model * vec4(a_position, 1.0 + "
            "float(a_instance_id & 0u)));\n"
            "}\n",
            ShaderType::VERTEX);

        auto fragment_shader = ShaderSource(
            "#version 450 core\n"
            "layout(location = 0) out vec4 o_color;\n"
            "void main()\n"
            "{\n"
            "    o_color = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "}\n",
            ShaderType::FRAGMENT);

        auto sources = std::vector<ShaderSource>();
        sources.push_back(std::move(vertex_shader));
        sources.push_back(std::move(fragment_shader));
        return std::make_shared<Shader>(std::move(sources));
    }

    inline Mesh make_two_sided_fallback_mesh()
    {
        auto mesh = Mesh(quad.vertices, quad.indices);
        const auto original_index_count = mesh.indices.size();
        mesh.indices.reserve(original_index_count * 2U);
        for (size_t index = 0U; index + 2U < original_index_count; index += 3U)
        {
            mesh.indices.push_back(mesh.indices[index]);
            mesh.indices.push_back(mesh.indices[index + 2U]);
            mesh.indices.push_back(mesh.indices[index + 1U]);
        }
        return mesh;
    }

    inline std::shared_ptr<Model> make_fallback_model()
    {
        auto material = Material();
        material.textures.set("diffuse_map", NotFoundIcon::HANDLE);
        return std::make_shared<Model>(make_two_sided_fallback_mesh(), material);
    }

    template <typename TAsset>
    std::shared_ptr<TAsset> make_fallback_asset(const AssetLoadParameters<TAsset>&)
    {
        return {};
    }

    template <>
    inline std::shared_ptr<AudioClip> make_fallback_asset<AudioClip>(const AudioLoadParameters&)
    {
        return make_fallback_audio_clip();
    }

    template <>
    inline std::shared_ptr<Material> make_fallback_asset<Material>(
        const MaterialLoadParameters&)
    {
        return make_fallback_material();
    }

    template <>
    inline std::shared_ptr<Texture> make_fallback_asset<Texture>(
        const TextureLoadParameters& parameters)
    {
        return make_fallback_texture(parameters);
    }

    template <>
    inline std::shared_ptr<Shader> make_fallback_asset<Shader>(const ShaderLoadParameters&)
    {
        return make_fallback_shader();
    }

    template <>
    inline std::shared_ptr<Model> make_fallback_asset<Model>(const ModelLoadParameters&)
    {
        return make_fallback_model();
    }
}
