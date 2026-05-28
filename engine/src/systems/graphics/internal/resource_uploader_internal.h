#pragma once
#include "systems/assets/internal/fallbacks_internal.h"
#include "systems/graphics/internal/resource_manager_internal.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/resource_upload_caches.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace tbx::internal
{
    constexpr uint32 MAX_MATERIAL_UNIFORM_VECTORS = 64U;
    constexpr size UNIFORM_BUFFER_RING_SIZE = 3U;

    static bool uses_mesh_resource(const RenderingMeshUploadData& mesh, const Uuid& resource)
    {
        return mesh.vertex_buffer == resource || mesh.index_buffer == resource;
    }

    template <typename TKey>
    static void erase_uuid_cache_entry(std::unordered_map<TKey, Uuid>& cache, const Uuid& resource)
    {
        for (auto iterator = cache.begin(); iterator != cache.end();)
        {
            if (iterator->second == resource)
                iterator = cache.erase(iterator);
            else
                ++iterator;
        }
    }

    static void discard_cached_mesh_resource(MeshResourceCache& cache, const Uuid& resource)
    {
        for (auto iterator = cache.model_meshes.begin(); iterator != cache.model_meshes.end();)
        {
            const auto& meshes = iterator->second;
            const bool uses_resource = std::any_of(
                meshes.begin(),
                meshes.end(),
                [resource](const RenderingMeshUploadData& mesh)
                {
                    return uses_mesh_resource(mesh, resource);
                });
            if (uses_resource)
                iterator = cache.model_meshes.erase(iterator);
            else
                ++iterator;
        }

        for (auto iterator = cache.runtime_meshes.begin(); iterator != cache.runtime_meshes.end();)
        {
            if (uses_mesh_resource(iterator->second, resource))
                iterator = cache.runtime_meshes.erase(iterator);
            else
                ++iterator;
        }

        for (auto iterator = cache.dynamic_meshes.begin(); iterator != cache.dynamic_meshes.end();)
        {
            if (uses_mesh_resource(iterator->second.mesh, resource))
                iterator = cache.dynamic_meshes.erase(iterator);
            else
                ++iterator;
        }
    }

    static void discard_cached_uniform_resource(UniformBufferCache& cache, const Uuid& resource)
    {
        for (auto cache_iterator = cache.uniform_buffers.begin();
             cache_iterator != cache.uniform_buffers.end();)
        {
            auto& ring = cache_iterator->second;
            for (auto& entry : ring)
            {
                if (entry.resource == resource)
                    entry = UniformBufferCacheEntry {};
            }

            const bool is_empty = std::all_of(
                ring.begin(),
                ring.end(),
                [](const UniformBufferCacheEntry& entry)
                {
                    return !entry.resource.is_valid();
                });
            if (is_empty)
                cache_iterator = cache.uniform_buffers.erase(cache_iterator);
            else
                ++cache_iterator;
        }
    }

    static bool is_same_binding_key(const ResourceBinding& left, const ResourceBinding& right)
    {
        return left.binding_slot == right.binding_slot
               && left.resource_handle == right.resource_handle && left.offset == right.offset
               && left.range == right.range;
    }

    static bool is_same_bind_group_key(const BindGroupDesc& left, const BindGroupDesc& right)
    {
        if (left.layout_handle != right.layout_handle
            || left.bindings.size() != right.bindings.size())
            return false;

        for (size index = 0U; index < left.bindings.size(); ++index)
            if (!is_same_binding_key(left.bindings[index], right.bindings[index]))
                return false;

        return true;
    }

    static bool bind_group_uses_resource(
        const RenderingBindGroupCacheEntry& entry,
        const Uuid& resource)
    {
        if (entry.resource == resource)
            return true;

        return std::any_of(
            entry.desc.bindings.begin(),
            entry.desc.bindings.end(),
            [resource](const ResourceBinding& binding)
            {
                return binding.resource_handle == resource;
            });
    }

    static uint64 make_bind_group_cache_hash(const BindGroupDesc& desc)
    {
        uint64 result = hash(desc.layout_handle);
        for (const auto& binding : desc.bindings)
        {
            result = hash(binding.binding_slot, result);
            result = hash(binding.resource_handle, result);
            result = hash(binding.offset, result);
            result = hash(binding.range, result);
        }

        return result == 0U ? 1U : result;
    }

    static void discard_cached_bind_group_resource(
        RenderingBindGroupCache& cache,
        const Uuid& resource)
    {
        for (auto cache_iterator = cache.bind_groups.begin();
             cache_iterator != cache.bind_groups.end();)
        {
            auto& entries = cache_iterator->second;
            const auto removed = std::remove_if(
                entries.begin(),
                entries.end(),
                [resource](const RenderingBindGroupCacheEntry& entry)
                {
                    return bind_group_uses_resource(entry, resource);
                });
            entries.erase(removed, entries.end());

            if (entries.empty())
                cache_iterator = cache.bind_groups.erase(cache_iterator);
            else
                ++cache_iterator;
        }
    }

    static void track_bind_group_resources(
        RenderingResourceTracker& resource_tracker,
        const BindGroupDesc& desc)
    {
        for (const auto& binding : desc.bindings)
            resource_tracker.track(binding.resource_handle);
    }

    static bool material_upload_uses_resource(
        const RenderingMaterialUploadData& material,
        const Uuid& resource)
    {
        if (material.pipeline == resource)
            return true;

        return std::any_of(
            material.textures.begin(),
            material.textures.end(),
            [resource](const GraphicsResourceBinding& binding)
            {
                return binding.resource == resource;
            });
    }

    static void discard_cached_material_resource(MaterialResourceCache& cache, const Uuid& resource)
    {
        for (auto iterator = cache.materials.begin(); iterator != cache.materials.end();)
        {
            if (material_upload_uses_resource(iterator->second, resource))
                iterator = cache.materials.erase(iterator);
            else
                ++iterator;
        }
    }

    static uint64 make_material_upload_cache_key(
        const Handle& material_handle,
        const MaterialInstance* instance)
    {
        uint64 result = hash(material_handle.id);
        result = hash(material_handle.name, result);
        if (instance == nullptr)
            return result == 0U ? 1U : result;

        result =
            hash(static_cast<uint64>(instance->has_config_override_enabled() ? 1U : 0U), result);
        if (instance->has_config_override_enabled())
            result = hash(instance->overrides.config, result);

        result =
            hash(static_cast<uint64>(instance->overrides.has_parameter_override ? 1U : 0U), result);
        for (const auto& parameter : instance->overrides.parameters)
        {
            result = hash(parameter.id, result);
            result = hash(parameter.data, result);
        }

        result =
            hash(static_cast<uint64>(instance->overrides.has_texture_override ? 1U : 0U), result);
        for (const auto& texture : instance->overrides.textures)
        {
            result = hash(texture.id, result);
            result = hash(texture.texture.id, result);
            result = hash(texture.texture.name, result);
        }

        return result == 0U ? 1U : result;
    }

    static void track_material_upload_resources(
        RenderingResourceTracker& resource_tracker,
        const RenderingMaterialUploadData& material)
    {
        resource_tracker.track(material.pipeline);
        for (const auto& texture : material.textures)
            resource_tracker.track(texture.resource);
    }

    static void append_instance_layout_attributes(
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        for (uint32 column = 0U; column < 4U; ++column)
        {
            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = VERTEX_ATTRIBUTE_INSTANCE_MODEL + column,
                    .buffer_slot = VERTEX_BUFFER_SLOT_INSTANCE,
                    .offset = static_cast<uint32>(sizeof(float) * 4U * column),
                    .format = GraphicsVertexFormat::VEC4,
                });
        }

        for (uint32 column = 0U; column < 4U; ++column)
        {
            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = VERTEX_ATTRIBUTE_INSTANCE_NORMAL + column,
                    .buffer_slot = VERTEX_BUFFER_SLOT_INSTANCE,
                    .offset = static_cast<uint32>(sizeof(Mat4) + (sizeof(float) * 4U * column)),
                    .format = GraphicsVertexFormat::VEC4,
                });
        }
    }

    static bool try_get_vertex_attribute_location(
        const std::string_view debug_name,
        uint32& out_location)
    {
        if (debug_name == vertex_attribute_position_debug_name)
        {
            out_location = VERTEX_ATTRIBUTE_POSITION;
            return true;
        }
        if (debug_name == vertex_attribute_normal_debug_name)
        {
            out_location = VERTEX_ATTRIBUTE_NORMAL;
            return true;
        }
        if (debug_name == vertex_attribute_tangent_debug_name)
        {
            out_location = VERTEX_ATTRIBUTE_TANGENT;
            return true;
        }
        if (debug_name == vertex_attribute_uv_debug_name)
        {
            out_location = VERTEX_ATTRIBUTE_TEX_COORD;
            return true;
        }
        if (debug_name == vertex_attribute_color_debug_name)
        {
            out_location = VERTEX_ATTRIBUTE_COLOR;
            return true;
        }

        return false;
    }

    static void append_vertex_layout_attributes(
        const VertexBufferLayout& layout,
        std::vector<GraphicsVertexAttributeDesc>& out_attributes)
    {
        const uint32 attribute_count = static_cast<uint32>(layout.elements.size());
        for (uint32 attribute_index = 0U; attribute_index < attribute_count; ++attribute_index)
        {
            const auto& attribute = layout.elements[static_cast<size>(attribute_index)];
            auto location = uint32 {};
            if (!try_get_vertex_attribute_location(attribute.debug_name, location))
                continue;

            out_attributes.push_back(
                GraphicsVertexAttributeDesc {
                    .location = location,
                    .buffer_slot = VERTEX_BUFFER_SLOT_MESH,
                    .offset = attribute.offset,
                    .format = attribute.type,
                });
        }
    }

    static std::vector<BindGroupLayoutDesc> make_material_bind_group_layouts()
    {
        constexpr uint32 graphics_stages = SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT;
        return std::vector<BindGroupLayoutDesc> {
            BindGroupLayoutDesc {
                .entries =
                    {
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_FRAME_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = graphics_stages,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_CAMERA_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = graphics_stages,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_LIGHT_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_SHADOW_PASS_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = graphics_stages,
                        },
                    },
                .debug_name = "Toybox Frame Bind Group Layout",
            },
            BindGroupLayoutDesc {
                .entries =
                    {
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_OBJECT_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = graphics_stages,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_MATERIAL_DATA,
                            .type = BindingType::UNIFORM_BUFFER,
                            .shader_stages = graphics_stages,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_ALBEDO_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_NORMAL_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_METALLIC_ROUGHNESS_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_AO_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_EMISSIVE_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_ALBEDO,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_NORMAL,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_MATERIAL,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_EMISSIVE,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_DEPTH,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_GBUFFER_FINAL_COLOR,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                        BindGroupLayoutEntry {
                            .binding_slot = BINDING_SHADOW_MAP,
                            .type = BindingType::SAMPLED_TEXTURE,
                            .shader_stages = SHADER_STAGE_FRAGMENT,
                        },
                    },
                .debug_name = "Toybox Material Bind Group Layout",
            },
        };
    }

    static RasterPipelineDesc make_material_pipeline_desc(
        const Handle& handle,
        ShaderProgram shader,
        const MaterialConfig& config)
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);
        append_instance_layout_attributes(vertex_attributes);

        return RasterPipelineDesc {
            .shader = std::move(shader),
            .bind_group_layouts = make_material_bind_group_layouts(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = VERTEX_BUFFER_SLOT_MESH,
                        .stride = vertex_layout.stride,
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = VERTEX_BUFFER_SLOT_INSTANCE,
                        .stride = static_cast<uint32>(sizeof(Mat4) * 2U),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes = std::move(vertex_attributes),
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = config.is_depth_test_enabled,
            .is_depth_write_enabled = config.is_depth_write_enabled,
            .is_blending_enabled = config.blend_mode == MaterialBlendMode::ALPHA_BLEND,
            .is_culling_enabled = config.is_cullable && !config.is_two_sided,
            .debug_name = std::format("Material {}", handle),
        };
    }

    static void append_parameter_uniform_data(
        const MaterialParameterData& parameter,
        std::vector<Vec4>& out_values)
    {
        std::visit(
            [&out_values](const auto& value)
            {
                using TValue = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<TValue, bool>)
                    out_values.push_back(Vec4(value ? 1.0F : 0.0F, 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, int> || std::is_same_v<TValue, double>)
                    out_values.push_back(Vec4(static_cast<float>(value), 0.0F, 0.0F, 0.0F));
                else if constexpr (std::is_same_v<TValue, float>)
                    out_values.push_back(Vec4(value, 0.0F, 0.0F, 0.0F));
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

    static std::vector<Vec4> make_material_uniform_values(
        const MaterialParameterBindings& parameters)
    {
        auto values = std::vector<Vec4> {};
        values.reserve(parameters.values.size());
        for (const auto& parameter : parameters)
            append_parameter_uniform_data(parameter.data, values);

        if (values.size() > MAX_MATERIAL_UNIFORM_VECTORS)
            values.resize(MAX_MATERIAL_UNIFORM_VECTORS);
        else if (values.size() < MAX_MATERIAL_UNIFORM_VECTORS)
            values.resize(MAX_MATERIAL_UNIFORM_VECTORS, Vec4(0.0F));

        return values;
    }

    static Handle make_default_material_handle()
    {
        return Handle("Materials/Pbr.mat", PbrMaterial::HANDLE.id);
    }

    static Handle resolve_material_handle(const MaterialInstance& instance)
    {
        auto material_handle = instance.get_handle();
        if ((!material_handle.id.is_valid() && material_handle.name.empty())
            || (material_handle.name.empty() && material_handle.id == PbrMaterial::HANDLE.id))
        {
            material_handle = make_default_material_handle();
        }

        return material_handle;
    }

    static MaterialConfig resolve_material_config(
        const Material& material,
        const MaterialInstance& instance)
    {
        return instance.has_config_override_enabled() ? instance.overrides.config : material.config;
    }

    static std::string make_material_pipeline_cache_key(
        const Handle& handle,
        const MaterialConfig& config)
    {
        auto key = std::format("{}", handle);
        key += "|depth_test=" + std::to_string(config.is_depth_test_enabled ? 1 : 0);
        key += "|depth_write=" + std::to_string(config.is_depth_write_enabled ? 1 : 0);
        key += "|two_sided=" + std::to_string(config.is_two_sided ? 1 : 0);
        key += "|cullable=" + std::to_string(config.is_cullable ? 1 : 0);
        key += "|blend=" + std::to_string(static_cast<int>(config.blend_mode));
        key += "|depth=" + std::to_string(static_cast<int>(config.depth_function));
        return key;
    }

    static Texture make_solid_texture(const Pixel r, const Pixel g, const Pixel b, const Pixel a)
    {
        return Texture(
            Size(1U, 1U),
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            TextureFormat::RGBA,
            TextureMipmaps::DISABLED,
            TextureCompression::DISABLED,
            std::vector<Pixel> {r, g, b, a});
    }

    static Texture make_default_texture_for_binding(const uint32 binding_id)
    {
        if (binding_id == PARAM_NORMAL_MAP)
        {
            return make_solid_texture(
                static_cast<Pixel>(128U),
                static_cast<Pixel>(128U),
                static_cast<Pixel>(255U),
                static_cast<Pixel>(255U));
        }

        if (binding_id == PARAM_EMISSIVE_MAP || binding_id == PARAM_SHADOW_MASK
            || binding_id == PARAM_GBUFFER_DEPTH)
        {
            return make_solid_texture(
                static_cast<Pixel>(0U),
                static_cast<Pixel>(0U),
                static_cast<Pixel>(0U),
                static_cast<Pixel>(255U));
        }

        if (binding_id == PARAM_METALLIC_ROUGHNESS_MAP)
        {
            return make_solid_texture(
                static_cast<Pixel>(0U),
                static_cast<Pixel>(255U),
                static_cast<Pixel>(0U),
                static_cast<Pixel>(255U));
        }

        return make_solid_texture(
            static_cast<Pixel>(255U),
            static_cast<Pixel>(255U),
            static_cast<Pixel>(255U),
            static_cast<Pixel>(255U));
    }

    static uint64 get_texture_channel_count(const TextureFormat format)
    {
        return format == TextureFormat::RGB ? 3U : 4U;
    }

    static uint64 get_texture_pixel_count(const Texture& texture)
    {
        return static_cast<uint64>(texture.resolution.width)
               * static_cast<uint64>(texture.resolution.height);
    }

    static uint64 get_texture_source_byte_size(const Texture& texture)
    {
        return get_texture_pixel_count(texture) * get_texture_channel_count(texture.format);
    }

    static GraphicsTextureDesc make_texture_desc(const Texture& texture, const Handle& handle)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED,
            .format = GraphicsTextureFormat::RGBA8,
            .size = texture.resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = std::format("Texture {}", handle),
        };
    }

    static std::vector<uint8> make_texture_upload_data(const Texture& texture)
    {
        if (texture.pixels.empty())
            return {};

        if (texture.format != TextureFormat::RGB)
            return std::vector<uint8>(texture.pixels.begin(), texture.pixels.end());

        auto upload_data = std::vector<uint8> {};
        upload_data.reserve(static_cast<size>(get_texture_pixel_count(texture) * 4U));
        const uint64 pixel_data_size = static_cast<uint64>(texture.pixels.size());
        for (uint64 source_index = 0U; source_index + 2U < pixel_data_size; source_index += 3U)
        {
            upload_data.push_back(texture.pixels[static_cast<size>(source_index)]);
            upload_data.push_back(texture.pixels[static_cast<size>(source_index + 1U)]);
            upload_data.push_back(texture.pixels[static_cast<size>(source_index + 2U)]);
            upload_data.push_back(static_cast<Pixel>(255U));
        }

        return upload_data;
    }

    static bool append_shader_sources(
        AssetManager& asset_manager,
        const Handle& handle,
        std::vector<Uuid>& loaded_shader_ids,
        std::vector<ShaderSource>& shader_sources)
    {
        if (!handle.is_valid())
            return true;

        const Uuid asset_id = asset_manager.ensure(handle);
        if (!asset_id.is_valid())
            return false;

        for (const Uuid loaded_shader_id : loaded_shader_ids)
            if (loaded_shader_id == asset_id)
                return true;

        const std::shared_ptr<ShaderProgram> shader =
            asset_manager.load<ShaderProgram>(handle, ShaderLoadParameters());
        if (!shader)
            return false;

        loaded_shader_ids.push_back(asset_id);
        shader_sources.insert(shader_sources.end(), shader->sources.begin(), shader->sources.end());
        return true;
    }

    static ShaderProgram build_material_shader(
        AssetManager& asset_manager,
        const Handle& handle,
        const Material& material,
        const ShaderProgram* fallback_shader)
    {
        auto shader_sources = std::vector<ShaderSource> {};
        auto loaded_shader_ids = std::vector<Uuid> {};
        auto has_shader_failure = false;

        if (material.shader.compute.id.is_valid())
        {
            has_shader_failure = !append_shader_sources(
                asset_manager,
                material.shader.compute,
                loaded_shader_ids,
                shader_sources);
        }
        else
        {
            if (!append_shader_sources(
                    asset_manager,
                    material.shader.vertex,
                    loaded_shader_ids,
                    shader_sources))
                has_shader_failure = true;
            if (!append_shader_sources(
                    asset_manager,
                    material.shader.fragment,
                    loaded_shader_ids,
                    shader_sources))
                has_shader_failure = true;
            if (!append_shader_sources(
                    asset_manager,
                    material.shader.tesselation,
                    loaded_shader_ids,
                    shader_sources))
                has_shader_failure = true;
            if (!append_shader_sources(
                    asset_manager,
                    material.shader.geometry,
                    loaded_shader_ids,
                    shader_sources))
                has_shader_failure = true;
        }

        if (!has_shader_failure && !shader_sources.empty())
            return ShaderProgram(std::move(shader_sources));

        TBX_TRACE_WARNING_ONCE(
            "Material '{}' failed to load one or more shader stages. Falling back to non-shaded "
            "magenta shader.",
            handle);
        return fallback_shader == nullptr ? ShaderProgram() : *fallback_shader;
    }

    static GraphicsBufferDesc make_uniform_buffer_desc(
        const std::string& debug_name,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::UNIFORM,
            .size = byte_size,
            .is_dynamic = true,
            .debug_name = debug_name,
        };
    }

    static GraphicsBufferDesc make_instance_buffer_desc(
        const std::string& debug_name,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::VERTEX,
            .size = byte_size,
            .is_dynamic = true,
            .debug_name = debug_name,
        };
    }

    static GraphicsBufferDesc make_mesh_vertex_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::VERTEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::format("Model {} Mesh {} Vertices", handle, mesh_index),
        };
    }

    static GraphicsBufferDesc make_mesh_index_buffer_desc(
        const Handle& handle,
        const uint mesh_index,
        const uint64 byte_size)
    {
        return GraphicsBufferDesc {
            .usage = GraphicsBufferUsage::INDEX,
            .size = byte_size,
            .is_dynamic = false,
            .debug_name = std::format("Model {} Mesh {} Indices", handle, mesh_index),
        };
    }

    static Uuid upload_buffer(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        const GraphicsBufferDesc& desc,
        const void* data,
        const uint64 data_size)
    {
        auto resource = Uuid {};
        auto result = backend.create_buffer(desc, resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering buffer allocation failed: {}", result.get_report());
            return {};
        }

        if (data_size > 0U)
        {
            result = backend.write_buffer(resource, data, data_size, 0U);
            if (!result)
            {
                TBX_TRACE_ERROR_ONCE("Rendering buffer write failed: {}", result.get_report());
                (void)backend.destroy_resource(resource);
                return {};
            }
        }

        resource_tracker.track(resource);
        return resource;
    }

    static std::optional<GraphicsResourceBinding> upload_fallback_texture(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        uint32 binding_id,
        std::unordered_map<uint32, Texture>& fallback_textures,
        TextureResourceCache& cache);

    static MaterialParameterBindings make_material_parameter_bindings(
        const MaterialParameterBindings& source)
    {
        auto bindings = MaterialParameterBindings {};
        for (const auto& parameter : source)
            bindings.set(parameter);
        return bindings;
    }

    static MaterialTextureBindings make_material_texture_bindings(
        const MaterialTextureBindings& source)
    {
        auto bindings = MaterialTextureBindings {};
        for (const auto& texture : source)
            bindings.set(texture);
        return bindings;
    }

    static std::optional<GraphicsResourceBinding> upload_material_texture(
        IGraphicsBackend& backend,
        AssetManager& asset_manager,
        RenderingResourceTracker& resource_tracker,
        const MaterialTextureBinding& binding,
        std::unordered_map<uint32, Texture>& fallback_textures,
        TextureResourceCache& cache)
    {
        const auto slot = resolve_shader_texture_slot(binding.id);
        auto handle = binding.texture;
        auto source_texture = std::shared_ptr<Texture> {};
        if (!binding.texture.id.is_valid())
            return upload_fallback_texture(
                backend,
                resource_tracker,
                binding.id,
                fallback_textures,
                cache);

        if (const auto cached = cache.textures.find(binding.texture);
            cached != cache.textures.end())
        {
            resource_tracker.track(cached->second);
            if (!slot.has_value())
                return std::nullopt;

            return GraphicsResourceBinding {.slot = *slot, .resource = cached->second};
        }

        source_texture = asset_manager.load<Texture>(binding.texture, TextureLoadParameters());
        if (!source_texture)
            return upload_fallback_texture(
                backend,
                resource_tracker,
                binding.id,
                fallback_textures,
                cache);

        if (!source_texture)
            return std::nullopt;

        const uint64 source_byte_size = get_texture_source_byte_size(*source_texture);
        if (source_texture->resolution.width == 0U || source_texture->resolution.height == 0U
            || (source_byte_size > 0U
                && static_cast<uint64>(source_texture->pixels.size()) < source_byte_size))
        {
            return upload_fallback_texture(
                backend,
                resource_tracker,
                binding.id,
                fallback_textures,
                cache);
        }

        const auto upload_data = make_texture_upload_data(*source_texture);
        auto resource = Uuid {};
        auto result = backend.create_texture(make_texture_desc(*source_texture, handle), resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture allocation failed: {}", result.get_report());
            return std::nullopt;
        }
        if (!upload_data.empty())
        {
            result = backend.write_texture(
                resource,
                GraphicsTextureUpdateDesc {
                    .width = source_texture->resolution.width,
                    .height = source_texture->resolution.height,
                },
                upload_data.data(),
                static_cast<uint64>(upload_data.size()));
            if (!result)
            {
                TBX_TRACE_ERROR_ONCE("Rendering texture write failed: {}", result.get_report());
                (void)backend.destroy_resource(resource);
                return std::nullopt;
            }
        }

        resource_tracker.track(resource);
        cache.textures[binding.texture] = resource;

        if (!slot.has_value())
            return std::nullopt;

        return GraphicsResourceBinding {.slot = *slot, .resource = resource};
    }

    static std::optional<GraphicsResourceBinding> upload_fallback_texture(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        const uint32 binding_id,
        std::unordered_map<uint32, Texture>& fallback_textures,
        TextureResourceCache& cache)
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        if (const auto cached = cache.default_textures.find(binding_id);
            cached != cache.default_textures.end())
        {
            resource_tracker.track(cached->second);
            if (!slot.has_value())
                return std::nullopt;

            return GraphicsResourceBinding {.slot = *slot, .resource = cached->second};
        }

        const auto handle =
            Handle(std::string("Toybox/DefaultTexture/") + std::to_string(binding_id));
        const auto& source_texture =
            fallback_textures.try_emplace(binding_id, make_default_texture_for_binding(binding_id))
                .first->second;
        const auto upload_data = make_texture_upload_data(source_texture);
        auto resource = Uuid {};
        auto result = backend.create_texture(make_texture_desc(source_texture, handle), resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture allocation failed: {}", result.get_report());
            return std::nullopt;
        }
        if (!upload_data.empty())
        {
            result = backend.write_texture(
                resource,
                GraphicsTextureUpdateDesc {
                    .width = source_texture.resolution.width,
                    .height = source_texture.resolution.height,
                },
                upload_data.data(),
                static_cast<uint64>(upload_data.size()));
            if (!result)
            {
                TBX_TRACE_ERROR_ONCE("Rendering texture write failed: {}", result.get_report());
                (void)backend.destroy_resource(resource);
                return std::nullopt;
            }
        }

        resource_tracker.track(resource);
        cache.default_textures[binding_id] = resource;

        if (!slot.has_value())
            return std::nullopt;

        return GraphicsResourceBinding {.slot = *slot, .resource = resource};
    }

    static Result upload_material_resources(
        IGraphicsBackend& backend,
        AssetManager* asset_manager,
        const Handle& material_handle,
        const Material& material,
        const MaterialInstance* instance,
        RenderingResourceTracker& resource_tracker,
        const ShaderProgram* fallback_shader,
        std::unordered_map<uint32, Texture>& fallback_textures,
        ResourceUploadCaches& caches,
        RenderingMaterialUploadData& out_material)
    {
        auto parameters = make_material_parameter_bindings(material.parameters);
        auto textures = make_material_texture_bindings(material.textures);
        const auto config =
            instance == nullptr ? material.config : resolve_material_config(material, *instance);

        if (instance != nullptr && instance->overrides.has_parameter_override)
            for (const auto& parameter : instance->overrides.parameters)
                parameters.set(parameter);
        if (instance != nullptr && instance->overrides.has_texture_override)
            for (const auto& texture : instance->overrides.textures)
                textures.set(texture);

        auto pipeline = Uuid {};
        const std::string pipeline_cache_key =
            make_material_pipeline_cache_key(material_handle, config);
        if (const auto cached_pipeline = caches.pipelines.pipelines.find(pipeline_cache_key);
            cached_pipeline != caches.pipelines.pipelines.end())
        {
            pipeline = cached_pipeline->second;
            resource_tracker.track(pipeline);
        }
        else
        {
            auto shader = ShaderProgram();
            if (asset_manager != nullptr)
                shader = build_material_shader(
                    *asset_manager,
                    material_handle,
                    material,
                    fallback_shader);
            else if (fallback_shader != nullptr)
                shader = *fallback_shader;

            const RasterPipelineDesc pipeline_desc =
                make_material_pipeline_desc(material_handle, shader, config);

            const Result pipeline_result = backend.create_raster_pipeline(pipeline_desc, pipeline);
            if (!pipeline_result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Rendering pipeline upload failed: {}",
                    pipeline_result.get_report());
                return Result(false, "Resource uploader failed: material pipeline upload failed.");
            }
            resource_tracker.track(pipeline);
            caches.pipelines.pipelines[pipeline_cache_key] = pipeline;
        }

        out_material = RenderingMaterialUploadData {
            .pipeline = pipeline,
            .uniform_values = make_material_uniform_values(parameters),
        };
        out_material.textures.reserve(textures.values.size());
        for (const auto& texture : textures)
        {
            auto texture_binding = std::optional<GraphicsResourceBinding>();
            if (asset_manager != nullptr && texture.texture.id.is_valid())
            {
                texture_binding = upload_material_texture(
                    backend,
                    *asset_manager,
                    resource_tracker,
                    texture,
                    fallback_textures,
                    caches.textures);
            }
            else
            {
                texture_binding = upload_fallback_texture(
                    backend,
                    resource_tracker,
                    texture.id,
                    fallback_textures,
                    caches.textures);
            }

            if (texture_binding.has_value())
                out_material.textures.push_back(*texture_binding);
        }

        return {};
    }

    static std::optional<RenderingMeshUploadData> upload_mesh(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        const Handle& handle,
        const Mesh& mesh,
        const uint mesh_index)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
            return std::nullopt;

        const uint64 vertex_data_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const Uuid vertex_buffer = upload_buffer(
            backend,
            resource_tracker,
            make_mesh_vertex_buffer_desc(handle, mesh_index, vertex_data_size),
            mesh.vertices.data(),
            vertex_data_size);

        const uint64 index_data_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));
        const Uuid index_buffer = upload_buffer(
            backend,
            resource_tracker,
            make_mesh_index_buffer_desc(handle, mesh_index, index_data_size),
            mesh.indices.data(),
            index_data_size);

        if (!vertex_buffer.is_valid() || !index_buffer.is_valid())
            return std::nullopt;

        return RenderingMeshUploadData {
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
            .index_count = static_cast<uint32>(mesh.indices.size()),
            .vertex_byte_size = vertex_data_size,
            .index_byte_size = index_data_size,
        };
    }

    static bool try_update_mesh(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        const RenderingMeshUploadData& cached_mesh,
        const Mesh& mesh)
    {
        const uint64 vertex_data_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_data_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));
        if (cached_mesh.vertex_byte_size != vertex_data_size
            || cached_mesh.index_byte_size != index_data_size)
        {
            return false;
        }

        const Result vertex_result = backend.write_buffer(
            cached_mesh.vertex_buffer,
            mesh.vertices.data(),
            vertex_data_size,
            0U);
        if (!vertex_result)
            return false;

        const Result index_result = backend.write_buffer(
            cached_mesh.index_buffer,
            mesh.indices.data(),
            index_data_size,
            0U);
        if (!index_result)
            return false;

        resource_tracker.track(cached_mesh.vertex_buffer);
        resource_tracker.track(cached_mesh.index_buffer);
        return true;
    }

    static GraphicsResourceBinding upload_or_update_uniform_buffer(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        UniformBufferCache& cache,
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size)
    {
        if (byte_size == 0U || (data == nullptr && byte_size > 0U))
            return GraphicsResourceBinding {.slot = slot};

        auto& ring = cache.uniform_buffers[cache_key];
        if (ring.empty())
            ring.resize(UNIFORM_BUFFER_RING_SIZE);

        auto& cached_buffer = ring[static_cast<size>(frame_index % ring.size())];
        if (cached_buffer.resource.is_valid() && cached_buffer.byte_size == byte_size)
        {
            const Result update_result =
                backend.write_buffer(cached_buffer.resource, data, byte_size, 0U);
            if (update_result)
            {
                resource_tracker.track(cached_buffer.resource);
                return GraphicsResourceBinding {.slot = slot, .resource = cached_buffer.resource};
            }

            TBX_TRACE_WARNING_ONCE(
                "Rendering uniform buffer update failed for '{}'. {}",
                debug_name,
                update_result.get_report());
        }

        const Uuid resource = upload_buffer(
            backend,
            resource_tracker,
            make_uniform_buffer_desc(debug_name, byte_size),
            data,
            byte_size);
        cached_buffer = UniformBufferCacheEntry {.resource = resource, .byte_size = byte_size};
        return GraphicsResourceBinding {.slot = slot, .resource = resource};
    }

    static GraphicsResourceBinding upload_instance_vertex_buffer(
        IGraphicsBackend& backend,
        RenderingResourceTracker& resource_tracker,
        UniformBufferCache& cache,
        const uint32 slot,
        const std::string& debug_name,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size)
    {
        if (byte_size == 0U || (data == nullptr && byte_size > 0U))
            return GraphicsResourceBinding {.slot = slot};

        auto& ring = cache.uniform_buffers[cache_key];
        if (ring.empty())
            ring.resize(UNIFORM_BUFFER_RING_SIZE);

        auto& cached_buffer = ring[static_cast<size>(frame_index % ring.size())];
        if (cached_buffer.resource.is_valid() && cached_buffer.byte_size == byte_size)
        {
            const Result update_result =
                backend.write_buffer(cached_buffer.resource, data, byte_size, 0U);
            if (update_result)
            {
                resource_tracker.track(cached_buffer.resource);
                return GraphicsResourceBinding {.slot = slot, .resource = cached_buffer.resource};
            }
        }

        const Uuid resource = upload_buffer(
            backend,
            resource_tracker,
            make_instance_buffer_desc(debug_name, byte_size),
            data,
            byte_size);
        cached_buffer = UniformBufferCacheEntry {.resource = resource, .byte_size = byte_size};
        return GraphicsResourceBinding {.slot = slot, .resource = resource};
    }
}
