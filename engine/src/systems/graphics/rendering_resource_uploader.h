#pragma once
#include "rendering_resource_tracker.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/resource_upload_caches.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/assets/builtin_assets.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/shader.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tbx
{
    constexpr uint32 MAX_MATERIAL_UNIFORM_VECTORS = 64U;
    constexpr size UNIFORM_BUFFER_RING_SIZE = 3U;

    static std::shared_ptr<Material> make_fallback_material()
    {
        auto material = Material();
        material.shader.vertex = DefaultFlatVertexShader::HANDLE;
        material.shader.fragment = DefaultFlatFragmentShader::HANDLE;
        material.parameters.set("albedo_color", Color(1.0F, 0.0F, 1.0F, 1.0F));
        material.textures.set("albedo_map", {});
        material.config = MaterialConfig {
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_depth_prepass_enabled = false,
            .depth_function = MaterialDepthFunction::LESS,
            .blend_mode = MaterialBlendMode::OPAQUE,
        };
        return std::make_shared<Material>(std::move(material));
    }

    static std::shared_ptr<std::vector<Shader>> make_fallback_shader()
    {
        auto vertex_shader = Shader(
            "#version 450 core\n"
            "layout(location = 0) in vec3 a_position;\n"
            "layout(location = 5) in mat4 a_instance_model;\n"
            "layout(std140, binding = 1) uniform TbxCameraData\n"
            "{\n"
            "    mat4 u_view;\n"
            "    mat4 u_projection;\n"
            "    mat4 u_view_projection;\n"
            "    mat4 u_inverse_view;\n"
            "    mat4 u_inverse_projection;\n"
            "    vec4 u_camera_world_position;\n"
            "};\n"
            "layout(std140, binding = 2) uniform TbxObjectData\n"
            "{\n"
            "    mat4 u_model;\n"
            "    mat4 u_normal;\n"
            "};\n"
            "void main()\n"
            "{\n"
            "    gl_Position = u_view_projection * a_instance_model * vec4(a_position, 1.0);\n"
            "}\n",
            ShaderType::VERTEX);
        auto fragment_shader = Shader(
            "#version 450 core\n"
            "layout(location = 0) out vec4 o_gbuffer_albedo;\n"
            "layout(location = 1) out vec4 o_gbuffer_normal;\n"
            "layout(location = 2) out vec4 o_gbuffer_material;\n"
            "layout(location = 3) out vec4 o_gbuffer_emissive;\n"
            "void main()\n"
            "{\n"
            "    o_gbuffer_albedo = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "    o_gbuffer_normal = vec4(0.5, 0.5, 1.0, 1.0);\n"
            "    o_gbuffer_material = vec4(0.0, 1.0, 1.0, 1.0);\n"
            "    o_gbuffer_emissive = vec4(1.0, 0.0, 1.0, 1.0);\n"
            "}\n",
            ShaderType::FRAGMENT);
        auto sources = std::vector<Shader>();
        sources.push_back(std::move(vertex_shader));
        sources.push_back(std::move(fragment_shader));
        return std::make_shared<std::vector<Shader>>(std::move(sources));
    }

    static std::shared_ptr<Model> make_fallback_model()
    {
        auto material = Material();
        material.textures.set("albedo_map", NotFoundIcon::HANDLE);
        return std::make_shared<Model>(Mesh::CUBE, material);
    }

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
        if (instance == nullptr)
            return static_cast<uint64>(std::hash<Handle>()(material_handle));
        return static_cast<uint64>(std::hash<MaterialInstance>()(*instance));
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
        std::vector<Shader> shaders,
        const MaterialConfig& config)
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);
        append_instance_layout_attributes(vertex_attributes);
        return RasterPipelineDesc {
            .shaders = std::move(shaders),
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
        return Handle(PbrMaterial::HANDLE.id);
    }

    static bool has_asset_reference(const Handle& handle)
    {
        return handle.id.is_valid() || !handle.name.empty();
    }

    static bool is_matching_asset_handle(const Handle& cached, const Handle& reloaded)
    {
        if (reloaded.id.is_valid() && cached.id == reloaded.id)
            return true;

        return !reloaded.name.empty() && cached.name == reloaded.name;
    }

    template <typename TValue>
    static void erase_asset_cache_entry(
        std::unordered_map<Handle, TValue>& cache,
        const Handle& asset)
    {
        for (auto iterator = cache.begin(); iterator != cache.end();)
        {
            if (is_matching_asset_handle(iterator->first, asset))
                iterator = cache.erase(iterator);
            else
                ++iterator;
        }
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
        std::vector<Shader>& shader_sources)
    {
        if (!handle.is_valid())
            return true;
        const Uuid asset_id = asset_manager.ensure(handle);
        if (!asset_id.is_valid())
            return false;
        for (const Uuid loaded_shader_id : loaded_shader_ids)
            if (loaded_shader_id == asset_id)
                return true;
        const std::shared_ptr<Shader> shader =
            asset_manager.load<Shader>(handle, ShaderLoadParameters());
        if (!shader)
            return false;
        loaded_shader_ids.push_back(asset_id);
        shader_sources.push_back(*shader);
        return true;
    }

    static std::vector<Shader> build_material_shader(
        AssetManager& asset_manager,
        const Handle& handle,
        const Material& material,
        const std::vector<Shader>* fallback_shader)
    {
        auto shader_sources = std::vector<Shader> {};
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
            return shader_sources;
        TBX_TRACE_WARNING_ONCE(
            "Material '{}' failed to load one or more shader stages. Falling back to non-shaded "
            "magenta shader.",
            handle);
        return fallback_shader == nullptr ? std::vector<Shader> {} : *fallback_shader;
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
        {
            TBX_TRACE_WARNING(
                "Rendering texture '{}' failed to load for material binding '{}'. Using fallback "
                "texture.",
                binding.texture,
                binding.name);
            return upload_fallback_texture(
                backend,
                resource_tracker,
                binding.id,
                fallback_textures,
                cache);
        }
        if (!source_texture)
            return std::nullopt;
        auto source_byte_size = get_texture_source_byte_size(*source_texture);
        auto has_valid_pixels =
            source_texture->resolution.width != 0U && source_texture->resolution.height != 0U
            && (source_byte_size == 0U
                || static_cast<uint64>(source_texture->pixels.size()) >= source_byte_size);
        if (!has_valid_pixels && asset_manager.reload<Texture>(binding.texture))
        {
            source_texture = asset_manager.load<Texture>(binding.texture, TextureLoadParameters());
            if (source_texture)
            {
                source_byte_size = get_texture_source_byte_size(*source_texture);
                has_valid_pixels =
                    source_texture->resolution.width != 0U
                    && source_texture->resolution.height != 0U
                    && (source_byte_size == 0U
                        || static_cast<uint64>(source_texture->pixels.size()) >= source_byte_size);
            }
        }
        if (!source_texture || !has_valid_pixels)
        {
            TBX_TRACE_WARNING(
                "Rendering texture '{}' has invalid pixel data for material binding '{}'. Using "
                "fallback texture.",
                binding.texture,
                binding.name);
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
        const std::vector<Shader>* fallback_shader,
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
            auto shader = std::vector<Shader> {};
            if (asset_manager != nullptr)
                shader = build_material_shader(
                    *asset_manager,
                    material_handle,
                    material,
                    fallback_shader);
            else if (fallback_shader != nullptr)
                shader = *fallback_shader;
            const RasterPipelineDesc pipeline_desc =
                make_material_pipeline_desc(material_handle, std::move(shader), config);
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

    class RenderingResourceUploader final
    {
      public:
        RenderingResourceUploader(
            std::weak_ptr<IGraphicsBackend> backend,
            std::weak_ptr<AssetManager> asset_manager)
            : _backend(std::move(backend))
            , _asset_manager(std::move(asset_manager))
            , _fallback_material(make_fallback_material())
            , _fallback_model(make_fallback_model())
            , _fallback_shader(make_fallback_shader())
        {
        }

        ~RenderingResourceUploader() = default;

      public:
        RenderingResourceUploader(const RenderingResourceUploader&) = delete;
        RenderingResourceUploader& operator=(const RenderingResourceUploader&) = delete;
        RenderingResourceUploader(RenderingResourceUploader&&) noexcept = delete;
        RenderingResourceUploader& operator=(RenderingResourceUploader&&) noexcept = delete;

      public:
        void cache_model_bounds(const Handle& model_handle, const MeshBounds& bounds) const
        {
            _caches.meshes.cache_model_bounds(model_handle, bounds);
        }

        void discard_cached_resource(const Uuid& resource)
        {
            if (!resource.is_valid())
                return;
            discard_cached_bind_group_resource(_caches.bind_groups, resource);
            discard_cached_material_resource(_caches.materials, resource);
            erase_uuid_cache_entry(_caches.pipelines.pipelines, resource);
            discard_cached_mesh_resource(_caches.meshes, resource);
            erase_uuid_cache_entry(_caches.textures.textures, resource);
            erase_uuid_cache_entry(_caches.textures.default_textures, resource);
            erase_uuid_cache_entry(_caches.textures.render_targets, resource);
            discard_cached_uniform_resource(_caches.uniforms, resource);
            discard_cached_uniform_resource(_caches.instances, resource);
        }

        void discard_cached_asset(const Handle& asset) const
        {
            if (!has_asset_reference(asset))
                return;

            erase_asset_cache_entry(_caches.meshes.model_bounds, asset);
            erase_asset_cache_entry(_caches.meshes.model_meshes, asset);
            erase_asset_cache_entry(_caches.meshes.runtime_meshes, asset);
            erase_asset_cache_entry(_caches.textures.textures, asset);

            // Materials and shaders compile into shared pipelines/bind groups, so the cheap and
            // reliable V1 behavior is to rebuild material-derived GPU state after any asset fanout.
            _caches.materials.materials.clear();
            _caches.pipelines.pipelines.clear();
            _caches.bind_groups.bind_groups.clear();
        }

        bool try_get_model_bounds(const Handle& model_handle, MeshBounds& out_bounds) const
        {
            return _caches.meshes.try_get_model_bounds(model_handle, out_bounds);
        }

        bool try_get_static_runtime_mesh(
            const Handle& mesh_handle,
            RenderingResourceTracker& resource_tracker,
            RenderingMeshUploadData& out_mesh) const
        {
            const auto cached_mesh = _caches.meshes.runtime_meshes.find(mesh_handle);
            if (cached_mesh == _caches.meshes.runtime_meshes.end())
                return false;
            resource_tracker.track(cached_mesh->second.vertex_buffer);
            resource_tracker.track(cached_mesh->second.index_buffer);
            out_mesh = cached_mesh->second;
            return true;
        }

        Uuid upload_bind_group(
            const BindGroupDesc& desc,
            RenderingResourceTracker& resource_tracker) const
        {
            if (desc.bindings.empty())
                return {};
            const uint64 cache_hash = make_bind_group_cache_hash(desc);
            auto& entries = _caches.bind_groups.bind_groups[cache_hash];
            const auto cached = std::ranges::find_if(
                entries,
                [&desc](const RenderingBindGroupCacheEntry& entry)
                {
                    return is_same_bind_group_key(entry.desc, desc);
                });
            if (cached != entries.end())
            {
                track_bind_group_resources(resource_tracker, cached->desc);
                resource_tracker.track(cached->resource);
                return cached->resource;
            }
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering bind group upload skipped because graphics backend is unavailable.");
                return {};
            }
            auto out_bind_group = Uuid();
            const Result result = backend->create_bind_group(desc, out_bind_group);
            if (!result)
            {
                TBX_TRACE_WARNING(
                    "Rendering bind group '{}' upload failed: {}",
                    desc.debug_name,
                    result.get_report());
                return {};
            }
            track_bind_group_resources(resource_tracker, desc);
            resource_tracker.track(out_bind_group);
            entries.push_back(
                RenderingBindGroupCacheEntry {
                    .resource = out_bind_group,
                    .desc = desc,
                });
            return out_bind_group;
        }

        RenderingMeshUploadData upload_dynamic_mesh(
            const std::shared_ptr<DynamicMeshData>& mesh_data,
            RenderingResourceTracker& resource_tracker) const
        {
            if (!mesh_data)
            {
                TBX_TRACE_WARNING(
                    "Rendering dynamic mesh upload used fallback mesh because mesh data is "
                    "unavailable.");
                const auto fallback_meshes = upload_fallback_mesh(resource_tracker);
                return fallback_meshes.empty() ? RenderingMeshUploadData()
                                               : fallback_meshes.front();
            }
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering dynamic mesh upload skipped because graphics backend is "
                    "unavailable.");
                return {};
            }
            const Mesh& mesh = mesh_data->get_mesh();
            const DynamicMeshData* cache_key = mesh_data.get();
            auto cached_mesh = _caches.meshes.dynamic_meshes.find(cache_key);
            if (cached_mesh != _caches.meshes.dynamic_meshes.end()
                && cached_mesh->second.data.expired())
            {
                cached_mesh = _caches.meshes.dynamic_meshes.erase(cached_mesh);
            }
            if (cached_mesh != _caches.meshes.dynamic_meshes.end())
            {
                if (!mesh_data->is_dirty())
                {
                    resource_tracker.track(cached_mesh->second.mesh.vertex_buffer);
                    resource_tracker.track(cached_mesh->second.mesh.index_buffer);
                    return cached_mesh->second.mesh;
                }
                if (try_update_mesh(*backend, resource_tracker, cached_mesh->second.mesh, mesh))
                {
                    mesh_data->clear_dirty();
                    return cached_mesh->second.mesh;
                }
            }
            const auto uploaded_mesh =
                upload_mesh(*backend, resource_tracker, Handle("Toybox/DynamicMesh"), mesh, 0U);
            if (!uploaded_mesh.has_value())
            {
                TBX_TRACE_WARNING("Rendering dynamic mesh upload failed. Using fallback mesh.");
                const auto fallback_meshes = upload_fallback_mesh(resource_tracker);
                return fallback_meshes.empty() ? RenderingMeshUploadData()
                                               : fallback_meshes.front();
            }
            _caches.meshes.dynamic_meshes[cache_key] = DynamicMeshResourceCacheEntry {
                .data = mesh_data,
                .mesh = *uploaded_mesh,
            };
            mesh_data->clear_dirty();
            return *uploaded_mesh;
        }

        RenderingMaterialUploadData upload_fallback_material(
            RenderingResourceTracker& resource_tracker) const
        {
            auto out_material = RenderingMaterialUploadData();
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering fallback material upload skipped because graphics backend is "
                    "unavailable.");
                return out_material;
            }
            if (!_fallback_material)
            {
                TBX_TRACE_WARNING(
                    "Rendering fallback material upload skipped because fallback material is "
                    "unavailable.");
                return out_material;
            }
            const auto material_handle = Handle("Toybox/FallbackMaterial");
            const uint64 cache_key = make_material_upload_cache_key(material_handle, nullptr);
            if (const auto cached_material = _caches.materials.materials.find(cache_key);
                cached_material != _caches.materials.materials.end())
            {
                out_material = cached_material->second;
                track_material_upload_resources(resource_tracker, out_material);
                return out_material;
            }
            const Result result = upload_material_resources(
                *backend,
                nullptr,
                material_handle,
                *_fallback_material,
                nullptr,
                resource_tracker,
                _fallback_shader.get(),
                _fallback_textures,
                _caches,
                out_material);
            if (result)
                _caches.materials.materials[cache_key] = out_material;
            else
                TBX_TRACE_WARNING(
                    "Rendering fallback material upload failed: {}",
                    result.get_report());
            return out_material;
        }

        std::vector<RenderingMeshUploadData> upload_fallback_mesh(
            RenderingResourceTracker& resource_tracker) const
        {
            const auto fallback_mesh_handle = Handle("Toybox/FallbackMesh");
            if (const auto cached_mesh = _caches.meshes.runtime_meshes.find(fallback_mesh_handle);
                cached_mesh != _caches.meshes.runtime_meshes.end())
            {
                resource_tracker.track(cached_mesh->second.vertex_buffer);
                resource_tracker.track(cached_mesh->second.index_buffer);
                return {cached_mesh->second};
            }
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering fallback mesh upload skipped because graphics backend is "
                    "unavailable.");
                return {};
            }
            const auto mesh =
                upload_mesh(*backend, resource_tracker, fallback_mesh_handle, Mesh::CUBE, 0U);
            if (!mesh.has_value())
            {
                TBX_TRACE_WARNING("Rendering fallback mesh upload failed.");
                return {};
            }
            _caches.meshes.runtime_meshes[fallback_mesh_handle] = *mesh;
            return {*mesh};
        }

        GraphicsResourceBinding upload_fallback_texture(
            RenderingResourceTracker& resource_tracker,
            const uint32 binding_id) const
        {
            const auto slot = resolve_shader_texture_slot(binding_id);
            const auto backend = _backend.lock();
            if (!backend || !slot.has_value())
                return GraphicsResourceBinding {.slot = slot.value_or(0U)};
            const auto binding = ::tbx::upload_fallback_texture(
                *backend,
                resource_tracker,
                binding_id,
                _fallback_textures,
                _caches.textures);
            return binding.value_or(GraphicsResourceBinding {.slot = *slot});
        }

        GraphicsResourceBinding upload_instance_buffer(
            RenderingResourceTracker& resource_tracker,
            const std::string& cache_key,
            const uint64 frame_index,
            const void* data,
            const uint64 byte_size) const
        {
            const auto backend = _backend.lock();
            if (!backend)
                return GraphicsResourceBinding {.slot = VERTEX_BUFFER_SLOT_INSTANCE};
            return upload_instance_vertex_buffer(
                *backend,
                resource_tracker,
                _caches.instances,
                VERTEX_BUFFER_SLOT_INSTANCE,
                std::string("Instance Shader Data ") + cache_key,
                cache_key,
                frame_index,
                data,
                byte_size);
        }

        RenderingMaterialUploadData upload_material(
            const MaterialInstance& instance,
            RenderingResourceTracker& resource_tracker) const
        {
            auto out_material = RenderingMaterialUploadData();
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering material upload skipped because graphics backend is unavailable.");
                return out_material;
            }
            const auto asset_manager = _asset_manager.lock();
            auto material_handle = resolve_material_handle(instance);
            const uint64 cache_key = make_material_upload_cache_key(material_handle, &instance);
            if (const auto cached_material = _caches.materials.materials.find(cache_key);
                cached_material != _caches.materials.materials.end())
            {
                out_material = cached_material->second;
                track_material_upload_resources(resource_tracker, out_material);
                return out_material;
            }
            auto loaded_material =
                asset_manager && has_asset_reference(material_handle)
                    ? asset_manager->load<Material>(material_handle, MaterialLoadParameters())
                    : nullptr;
            if (!asset_manager)
            {
                TBX_TRACE_WARNING(
                    "Rendering material '{}' could not load because asset manager is unavailable. "
                    "Using bright magenta fallback material.",
                    material_handle);
            }
            else if (!has_asset_reference(material_handle))
            {
                TBX_TRACE_WARNING(
                    "Rendering material upload received an invalid handle. Using bright magenta "
                    "fallback material.");
            }
            if (!loaded_material)
            {
                if (asset_manager && has_asset_reference(material_handle))
                {
                    TBX_TRACE_WARNING(
                        "Rendering material '{}' failed to load. Using bright magenta fallback "
                        "material.",
                        material_handle);
                }
                loaded_material = _fallback_material;
            }
            if (!loaded_material)
                return upload_fallback_material(resource_tracker);
            const auto material = *loaded_material;
            loaded_material.reset();
            const Result result = upload_material_resources(
                *backend,
                asset_manager.get(),
                material_handle,
                material,
                &instance,
                resource_tracker,
                _fallback_shader.get(),
                _fallback_textures,
                _caches,
                out_material);
            if (result)
                _caches.materials.materials[cache_key] = out_material;
            else
            {
                TBX_TRACE_WARNING(
                    "Rendering material '{}' upload failed: {}. Using bright magenta fallback "
                    "material.",
                    material_handle,
                    result.get_report());
                out_material = upload_fallback_material(resource_tracker);
            }
            return out_material;
        }

        std::vector<RenderingMeshUploadData> upload_model_meshes(
            const Handle& model_handle,
            RenderingResourceTracker& resource_tracker) const
        {
            if (const auto cached_meshes = _caches.meshes.model_meshes.find(model_handle);
                cached_meshes != _caches.meshes.model_meshes.end())
            {
                auto out_meshes = std::vector<RenderingMeshUploadData>();
                for (const auto& mesh : cached_meshes->second)
                {
                    resource_tracker.track(mesh.vertex_buffer);
                    resource_tracker.track(mesh.index_buffer);
                    out_meshes.push_back(mesh);
                }
                return out_meshes;
            }
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering model '{}' upload skipped because graphics backend is unavailable.",
                    model_handle);
                return {};
            }
            const auto asset_manager = _asset_manager.lock();
            auto model = asset_manager && has_asset_reference(model_handle)
                             ? asset_manager->load<Model>(model_handle, ModelLoadParameters())
                             : nullptr;
            if (!asset_manager)
            {
                TBX_TRACE_WARNING(
                    "Rendering model '{}' could not load because asset manager is unavailable. "
                    "Using "
                    "fallback cube model.",
                    model_handle);
            }
            else if (!has_asset_reference(model_handle))
            {
                TBX_TRACE_WARNING(
                    "Rendering model upload received an invalid handle. Using fallback cube "
                    "model.");
            }
            if (!model)
            {
                if (asset_manager && has_asset_reference(model_handle))
                {
                    TBX_TRACE_WARNING(
                        "Rendering model '{}' failed to load. Using fallback cube model.",
                        model_handle);
                }
                model = _fallback_model;
            }
            if (!model)
                return upload_fallback_mesh(resource_tracker);
            auto out_meshes = std::vector<RenderingMeshUploadData>();
            for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model->meshes.size());
                 ++mesh_index)
            {
                const auto mesh = upload_mesh(
                    *backend,
                    resource_tracker,
                    model_handle,
                    model->meshes[static_cast<size>(mesh_index)],
                    mesh_index);
                if (mesh.has_value())
                {
                    out_meshes.push_back(*mesh);
                }
                else
                {
                    TBX_TRACE_WARNING(
                        "Rendering model '{}' mesh {} upload failed.",
                        model_handle,
                        mesh_index);
                }
            }
            if (!out_meshes.empty())
                _caches.meshes.model_meshes[model_handle] = out_meshes;
            else
                out_meshes = upload_fallback_mesh(resource_tracker);
            return out_meshes;
        }

        RenderingMeshUploadData upload_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker) const
        {
            const auto backend = _backend.lock();
            if (!backend)
            {
                TBX_TRACE_WARNING(
                    "Rendering runtime mesh '{}' upload skipped because graphics backend is "
                    "unavailable.",
                    mesh_handle);
                return {};
            }
            const auto mesh_data = upload_mesh(*backend, resource_tracker, mesh_handle, mesh, 0U);
            if (!mesh_data.has_value())
            {
                TBX_TRACE_WARNING(
                    "Rendering runtime mesh '{}' upload failed. Using fallback mesh.",
                    mesh_handle);
                const auto fallback_meshes = upload_fallback_mesh(resource_tracker);
                return fallback_meshes.empty() ? RenderingMeshUploadData()
                                               : fallback_meshes.front();
            }
            return *mesh_data;
        }

        RenderingMeshUploadData upload_static_runtime_mesh(
            const Handle& mesh_handle,
            const Mesh& mesh,
            RenderingResourceTracker& resource_tracker) const
        {
            auto out_mesh = RenderingMeshUploadData();
            if (try_get_static_runtime_mesh(mesh_handle, resource_tracker, out_mesh))
                return out_mesh;
            out_mesh = upload_runtime_mesh(mesh_handle, mesh, resource_tracker);
            if (out_mesh.vertex_buffer.is_valid() && out_mesh.index_buffer.is_valid())
                _caches.meshes.runtime_meshes[mesh_handle] = out_mesh;
            return out_mesh;
        }

        GraphicsResourceBinding upload_texture(
            RenderingResourceTracker& resource_tracker,
            const uint32 slot,
            const std::string& cache_key,
            const GraphicsTextureDesc& desc) const
        {
            const auto backend = _backend.lock();
            if (!backend)
                return GraphicsResourceBinding {.slot = slot};
            if (const auto cached = _caches.textures.render_targets.find(cache_key);
                cached != _caches.textures.render_targets.end())
            {
                resource_tracker.track(cached->second);
                return GraphicsResourceBinding {.slot = slot, .resource = cached->second};
            }
            auto resource = Uuid {};
            const Result result = backend->create_texture(desc, resource);
            if (!result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Rendering texture target upload failed: {}",
                    result.get_report());
                return GraphicsResourceBinding {.slot = slot};
            }
            resource_tracker.track(resource);
            _caches.textures.render_targets[cache_key] = resource;
            return GraphicsResourceBinding {.slot = slot, .resource = resource};
        }

        GraphicsResourceBinding upload_uniform_buffer(
            RenderingResourceTracker& resource_tracker,
            const uint32 slot,
            const std::string& debug_name,
            const std::string& cache_key,
            const uint64 frame_index,
            const void* data,
            const uint64 byte_size) const
        {
            const auto backend = _backend.lock();
            if (!backend)
                return GraphicsResourceBinding {.slot = slot};
            return upload_or_update_uniform_buffer(
                *backend,
                resource_tracker,
                _caches.uniforms,
                slot,
                debug_name,
                cache_key,
                frame_index,
                data,
                byte_size);
        }

      private:
        std::weak_ptr<AssetManager> _asset_manager = {};
        std::weak_ptr<IGraphicsBackend> _backend = {};
        std::shared_ptr<Material> _fallback_material = {};
        std::shared_ptr<Model> _fallback_model = {};
        std::shared_ptr<std::vector<Shader>> _fallback_shader = {};
        mutable ResourceUploadCaches _caches = {};
        mutable std::unordered_map<uint32, Texture> _fallback_textures = {};
    };
}
