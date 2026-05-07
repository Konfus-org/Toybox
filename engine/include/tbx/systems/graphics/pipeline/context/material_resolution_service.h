#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/pipeline/commands/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/tbx_api.h"
#include <type_traits>
#include <variant>
#include <vector>

namespace tbx
{
    inline constexpr uint32 TBX_MAX_MATERIAL_UNIFORM_VECTORS = 64U;

    struct MaterialUniformData
    {
        const void* data() const
        {
            return values.data();
        }

        uint64 byte_size() const
        {
            return static_cast<uint64>(values.size()) * static_cast<uint64>(sizeof(Vec4));
        }

        std::vector<Vec4> values = {};
    };

    // ---------------------------------------------------------------------------
    // FNV-1a hashing utilities
    // ---------------------------------------------------------------------------

    inline uint64 hash_bytes(
        const void* data,
        const uint64 size,
        uint64 hash = 14695981039346656037ULL)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (uint64 index = 0U; index < size; ++index)
        {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    inline uint64 hash_value(const uint64 value, const uint64 hash)
    {
        return hash_bytes(&value, static_cast<uint64>(sizeof(value)), hash);
    }

    inline uint64 hash_uuid(const Uuid value, const uint64 hash)
    {
        return hash_value(static_cast<uint32>(value), hash);
    }

    inline uint64 make_material_key(
        const Uuid pipeline,
        const MaterialUniformData& uniforms,
        const std::vector<GraphicsResourceBinding>& textures)
    {
        uint64 hash = hash_uuid(pipeline, 14695981039346656037ULL);
        hash = hash_value(static_cast<uint64>(uniforms.values.size()), hash);
        if (!uniforms.values.empty())
            hash = hash_bytes(uniforms.data(), uniforms.byte_size(), hash);
        for (const auto& texture : textures)
        {
            hash = hash_value(texture.slot, hash);
            hash = hash_uuid(texture.resource, hash);
        }
        return hash == 0U ? 1U : hash;
    }

    // ---------------------------------------------------------------------------
    // Material parameter packing helpers
    // ---------------------------------------------------------------------------

    inline void append_parameter_uniform_data(
        const MaterialParameterData& parameter,
        std::vector<Vec4>& out_values)
    {
        std::visit(
            [&out_values](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<TValue, bool>)
                    out_values.push_back(Vec4(value ? 1.0F : 0.0F, 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, int>)
                    out_values.push_back(Vec4(static_cast<float>(value), 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, float>)
                    out_values.push_back(Vec4(value, 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, double>)
                    out_values.push_back(Vec4(static_cast<float>(value), 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec2>)
                    out_values.push_back(Vec4(value, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec3>)
                    out_values.push_back(Vec4(value, 0.0F));
                else if constexpr (std::is_same_v<TValue, Vec4>)
                    out_values.push_back(value);
                else if constexpr (std::is_same_v<TValue, Color>)
                    out_values.push_back(Vec4(value.r, value.g, value.b, value.a));
                else if constexpr (std::is_same_v<TValue, Mat3>)
                {
                    out_values.push_back(Vec4(value[0], 0.0F));
                    out_values.push_back(Vec4(value[1], 0.0F));
                    out_values.push_back(Vec4(value[2], 0.0F));
                }
                else if constexpr (std::is_same_v<TValue, Mat4>)
                {
                    out_values.push_back(value[0]);
                    out_values.push_back(value[1]);
                    out_values.push_back(value[2]);
                    out_values.push_back(value[3]);
                }
            },
            parameter);
    }

    inline MaterialUniformData make_material_uniform_data(
        const MaterialParameterBindings& parameters)
    {
        auto uniform_data = MaterialUniformData {};
        uniform_data.values.reserve(parameters.values.size());
        for (const auto& parameter : parameters)
            append_parameter_uniform_data(parameter.data, uniform_data.values);

        if (uniform_data.values.size() > TBX_MAX_MATERIAL_UNIFORM_VECTORS)
            uniform_data.values.resize(TBX_MAX_MATERIAL_UNIFORM_VECTORS);
        else if (uniform_data.values.size() < TBX_MAX_MATERIAL_UNIFORM_VECTORS)
            uniform_data.values.resize(TBX_MAX_MATERIAL_UNIFORM_VECTORS, Vec4(0.0F));

        return uniform_data;
    }

    inline Result resolve_material_textures(
        const MaterialTextureBindings& texture_bindings,
        GraphicsResourceManager& resource_manager,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        auto texture_slot = uint32 {0U};
        out_textures.reserve(texture_bindings.values.size());
        for (const auto& texture : texture_bindings)
        {
            auto texture_resource = Uuid {};
            if (texture.texture.is_valid())
            {
                if (const auto result =
                        resource_manager.load_texture(texture.texture, texture_resource);
                    !result)
                    return result;
            }
            else if (const auto result = resource_manager.load_default_texture(texture_resource);
                     !result)
            {
                return result;
            }

            out_textures.push_back(
                GraphicsResourceBinding {
                    .slot = texture_slot,
                    .resource = texture_resource,
                });
            texture_slot += 1U;
        }
        return {};
    }

    // ---------------------------------------------------------------------------
    // Resolved material state — pipeline + packed uniform data + texture bindings
    // ---------------------------------------------------------------------------

    struct ResolvedMaterial
    {
        Uuid pipeline = {};
        uint64 uniform_key = 0U;
        MaterialUniformData uniform_data = {};
        std::vector<GraphicsResourceBinding> textures = {};
    };

    inline Result resolve_render_material(
        const MaterialInstance& material,
        GraphicsResourceManager& resource_manager,
        ResolvedMaterial& out)
    {
        out = {};
        auto material_resource = GraphicsMaterialInstanceResource {};
        if (const auto result =
                resource_manager.load_material_instance(material, material_resource);
            !result)
            return result;

        out.uniform_data = make_material_uniform_data(material_resource.parameters);
        if (const auto result = resolve_material_textures(
                material_resource.textures,
                resource_manager,
                out.textures);
            !result)
            return result;

        out.pipeline = material_resource.pipeline;
        out.uniform_key = make_material_key(out.pipeline, out.uniform_data, out.textures);
        return {};
    }

}
