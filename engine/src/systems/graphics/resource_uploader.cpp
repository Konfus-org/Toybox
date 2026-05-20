#include "tbx/systems/graphics/resource_uploader.h"
#include "tbx/systems/assets/fallbacks.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/model.h"
#include "tbx/types/material.h"
#include "tbx/types/texture.h"
#include "tbx/types/vertex.h"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

namespace tbx::detail
{
    constexpr uint32 MAX_MATERIAL_UNIFORM_VECTORS = 64U;
    constexpr size UNIFORM_BUFFER_RING_SIZE = 3U;

    static bool uses_mesh_resource(
        const RenderingMeshUploadData& mesh,
        const Uuid& resource)
    {
        return mesh.vertex_buffer == resource || mesh.index_buffer == resource;
    }

    template <typename TKey>
    static void erase_uuid_cache_entry(
        std::unordered_map<TKey, Uuid>& cache,
        const Uuid& resource)
    {
        for (auto iterator = cache.begin(); iterator != cache.end();)
        {
            if (iterator->second == resource)
                iterator = cache.erase(iterator);
            else
                ++iterator;
        }
    }

    static void discard_cached_mesh_resource(
        MeshResourceCache& cache,
        const Uuid& resource)
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

    static void discard_cached_uniform_resource(
        UniformBufferCache& cache,
        const Uuid& resource)
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

    static GraphicsPipelineDesc make_material_pipeline_desc(
        const Handle& handle,
        ShaderProgram shader,
        const MaterialConfig& config)
    {
        const VertexBufferLayout vertex_layout = get_default_vertex_buffer_layout();
        auto vertex_attributes = std::vector<GraphicsVertexAttributeDesc> {};
        append_vertex_layout_attributes(vertex_layout, vertex_attributes);
        append_instance_layout_attributes(vertex_attributes);

        return GraphicsPipelineDesc {
            .shader = std::move(shader),
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
            .is_blending_enabled = config.blend_mode == MaterialBlendMode::AlphaBlend,
            .is_culling_enabled = config.is_cullable && !config.is_two_sided,
            .debug_name = std::string("Material ") + to_string(handle),
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
        return Handle("Materials/Pbr.mat", PbrMaterial::HANDLE.get_id());
    }

    static std::string make_material_pipeline_cache_key(
        const Handle& handle,
        const MaterialConfig& config)
    {
        auto key = to_string(handle);
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

    static Texture make_default_texture_for_binding(const std::string_view binding_name)
    {
        if (binding_name == "u_normal_map")
        {
            return make_solid_texture(
                static_cast<Pixel>(128U),
                static_cast<Pixel>(128U),
                static_cast<Pixel>(255U),
                static_cast<Pixel>(255U));
        }

        if (binding_name == "u_emissive_map" || binding_name == "u_shadow_mask"
            || binding_name == "u_source_depth")
        {
            return make_solid_texture(
                static_cast<Pixel>(0U),
                static_cast<Pixel>(0U),
                static_cast<Pixel>(0U),
                static_cast<Pixel>(255U));
        }

        if (binding_name == "u_metallic_roughness_map")
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
            .debug_name = std::string("Texture ") + to_string(handle),
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
        const Material& material)
    {
        auto shader_sources = std::vector<ShaderSource> {};
        auto loaded_shader_ids = std::vector<Uuid> {};
        auto has_shader_failure = false;

        if (material.shader.compute.is_valid())
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
            to_string(handle));
        const auto fallback_shader = make_fallback_shader();
        return fallback_shader ? *fallback_shader : ShaderProgram();
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
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Vertices",
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
            .debug_name = std::string("Model ") + to_string(handle) + " Mesh "
                          + std::to_string(mesh_index) + " Indices",
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
        const Result result = backend.upload_buffer(desc, data, data_size, resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering buffer upload failed: {}", result.get_report());
            return {};
        }

        resource_tracker.track(resource);
        return resource;
    }

    static std::optional<GraphicsResourceBinding> upload_material_texture(
        IGraphicsBackend& backend,
        AssetManager& asset_manager,
        RenderingResourceTracker& resource_tracker,
        const MaterialTextureBinding& binding,
        TextureResourceCache& cache)
    {
        const auto slot = resolve_shader_texture_slot(binding.name);
        auto handle = binding.texture;
        auto source_texture = std::shared_ptr<Texture> {};
        if (binding.texture.is_valid())
        {
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
                source_texture = make_fallback_texture(TextureLoadParameters());
        }
        else
        {
            handle = Handle(std::string("Toybox/DefaultTexture/") + binding.name);
            if (const auto cached = cache.default_textures.find(binding.name);
                cached != cache.default_textures.end())
            {
                resource_tracker.track(cached->second);
                if (!slot.has_value())
                    return std::nullopt;

                return GraphicsResourceBinding {.slot = *slot, .resource = cached->second};
            }

            source_texture =
                std::make_shared<Texture>(make_default_texture_for_binding(binding.name));
        }

        if (!source_texture)
            return std::nullopt;

        const uint64 source_byte_size = get_texture_source_byte_size(*source_texture);
        if (source_texture->resolution.width == 0U || source_texture->resolution.height == 0U
            || (source_byte_size > 0U
                && static_cast<uint64>(source_texture->pixels.size()) < source_byte_size))
        {
            source_texture = make_fallback_texture(TextureLoadParameters());
        }

        const auto upload_data = make_texture_upload_data(*source_texture);
        auto resource = Uuid {};
        const Result result = backend.upload_texture(
            make_texture_desc(*source_texture, handle),
            upload_data.empty() ? nullptr : upload_data.data(),
            static_cast<uint64>(upload_data.size()),
            resource);
        if (!result)
        {
            TBX_TRACE_ERROR_ONCE("Rendering texture upload failed: {}", result.get_report());
            return std::nullopt;
        }

        resource_tracker.track(resource);
        if (binding.texture.is_valid())
            cache.textures[binding.texture] = resource;
        else
            cache.default_textures[binding.name] = resource;

        if (!slot.has_value())
            return std::nullopt;

        return GraphicsResourceBinding {.slot = *slot, .resource = resource};
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

        const Result vertex_result = backend.update_buffer(
            cached_mesh.vertex_buffer,
            mesh.vertices.data(),
            vertex_data_size,
            0U);
        if (!vertex_result)
            return false;

        const Result index_result =
            backend.update_buffer(cached_mesh.index_buffer, mesh.indices.data(), index_data_size, 0U);
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
                backend.update_buffer(cached_buffer.resource, data, byte_size, 0U);
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
                backend.update_buffer(cached_buffer.resource, data, byte_size, 0U);
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

namespace tbx
{
    ResourceUploader::ResourceUploader(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager)
        : _backend(std::move(backend))
        , _asset_manager(std::move(asset_manager))
    {
    }

    Result ResourceUploader::upload_fallback_mesh(
        RenderingResourceTracker& resource_tracker,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        const auto fallback_mesh_handle = Handle("Toybox/FallbackMesh");
        if (const auto cached_mesh = _caches.meshes.runtime_meshes.find(fallback_mesh_handle);
            cached_mesh != _caches.meshes.runtime_meshes.end())
        {
            resource_tracker.track(cached_mesh->second.vertex_buffer);
            resource_tracker.track(cached_mesh->second.index_buffer);
            out_meshes.push_back(cached_mesh->second);
            return {};
        }

        auto mesh = RenderingMeshUploadData();
        const Result result = upload_static_runtime_mesh(
            fallback_mesh_handle,
            make_cube(),
            resource_tracker,
            mesh);
        if (!result)
            return Result(false, "Resource uploader failed: fallback mesh upload failed.");

        out_meshes.push_back(mesh);
        return {};
    }

    Result ResourceUploader::upload_material(
        const MaterialInstance& instance,
        RenderingResourceTracker& resource_tracker,
        RenderingMaterialUploadData& out_material) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Resource uploader failed: asset manager unavailable.");

        auto material_handle = instance.get_handle();
        if (!material_handle.is_valid()
            || (material_handle.get_name().empty()
                && material_handle.get_id() == PbrMaterial::HANDLE.get_id()))
        {
            material_handle = detail::make_default_material_handle();
        }

        auto loaded_material =
            asset_manager->load<Material>(material_handle, MaterialLoadParameters());
        if (!loaded_material)
            loaded_material = make_fallback_material();
        if (!loaded_material)
            return Result(false, "Resource uploader failed: material load failed.");

        const auto material = *loaded_material;
        loaded_material.reset();

        auto parameters = material.parameters;
        auto textures = material.textures;
        auto config = material.config;

        if (instance.has_config_override_enabled())
            config = instance.config;
        for (const auto& parameter : instance.param_overrides)
            parameters.set(parameter);
        for (const auto& texture : instance.texture_overrides)
            textures.set(texture);

        auto pipeline = Uuid {};
        const std::string pipeline_cache_key =
            detail::make_material_pipeline_cache_key(material_handle, config);
        if (const auto cached_pipeline = _caches.pipelines.pipelines.find(pipeline_cache_key);
            cached_pipeline != _caches.pipelines.pipelines.end())
        {
            pipeline = cached_pipeline->second;
            resource_tracker.track(pipeline);
        }
        else
        {
            const ShaderProgram shader =
                detail::build_material_shader(*asset_manager, material_handle, material);
            const GraphicsPipelineDesc pipeline_desc =
                detail::make_material_pipeline_desc(material_handle, shader, config);

            const Result pipeline_result = backend->upload_pipeline(pipeline_desc, pipeline);
            if (!pipeline_result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Rendering pipeline upload failed: {}",
                    pipeline_result.get_report());
                return Result(false, "Resource uploader failed: material pipeline upload failed.");
            }
            resource_tracker.track(pipeline);
            _caches.pipelines.pipelines[pipeline_cache_key] = pipeline;
        }

        out_material = RenderingMaterialUploadData {
            .pipeline = pipeline,
            .uniform_values = detail::make_material_uniform_values(parameters),
        };
        out_material.textures.reserve(textures.values.size());
        for (const auto& texture : textures)
        {
            const auto texture_binding = detail::upload_material_texture(
                *backend,
                *asset_manager,
                resource_tracker,
                texture,
                _caches.textures);
            if (texture_binding.has_value())
                out_material.textures.push_back(*texture_binding);
        }

        return {};
    }

    Result ResourceUploader::upload_dynamic_mesh(
        const std::shared_ptr<DynamicMeshData>& mesh_data,
        RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        if (!mesh_data)
            return Result(false, "Resource uploader failed: dynamic mesh data is unavailable.");

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

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
                out_mesh = cached_mesh->second.mesh;
                return {};
            }

            if (detail::try_update_mesh(*backend, resource_tracker, cached_mesh->second.mesh, mesh))
            {
                mesh_data->clear_dirty();
                out_mesh = cached_mesh->second.mesh;
                return {};
            }
        }

        const auto uploaded_mesh =
            detail::upload_mesh(*backend, resource_tracker, Handle("Toybox/DynamicMesh"), mesh, 0U);
        if (!uploaded_mesh.has_value())
            return Result(false, "Resource uploader failed: dynamic mesh upload failed.");

        _caches.meshes.dynamic_meshes[cache_key] = DynamicMeshResourceCacheEntry {
            .data = mesh_data,
            .mesh = *uploaded_mesh,
        };
        mesh_data->clear_dirty();
        out_mesh = *uploaded_mesh;
        return {};
    }

    Result ResourceUploader::upload_model_meshes(
        const Handle& model_handle,
        RenderingResourceTracker& resource_tracker,
        std::vector<RenderingMeshUploadData>& out_meshes) const
    {
        if (const auto cached_meshes = _caches.meshes.model_meshes.find(model_handle);
            cached_meshes != _caches.meshes.model_meshes.end())
        {
            for (const auto& mesh : cached_meshes->second)
            {
                resource_tracker.track(mesh.vertex_buffer);
                resource_tracker.track(mesh.index_buffer);
                out_meshes.push_back(mesh);
            }
            return {};
        }

        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
            return Result(false, "Resource uploader failed: asset manager unavailable.");

        auto model = asset_manager->load<Model>(model_handle, ModelLoadParameters());
        if (!model)
            model = make_fallback_model();
        if (!model)
            return Result(false, "Resource uploader failed: model load failed.");

        for (uint mesh_index = 0U; mesh_index < static_cast<uint>(model->meshes.size());
             ++mesh_index)
        {
            const auto mesh = detail::upload_mesh(
                *backend,
                resource_tracker,
                model_handle,
                model->meshes[static_cast<size>(mesh_index)],
                mesh_index);
            if (mesh.has_value())
                out_meshes.push_back(*mesh);
        }

        if (!out_meshes.empty())
            _caches.meshes.model_meshes[model_handle] = out_meshes;

        return {};
    }

    Result ResourceUploader::upload_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        const auto backend = _backend.lock();
        if (!backend)
            return Result(false, "Resource uploader failed: graphics backend unavailable.");

        const auto mesh_data =
            detail::upload_mesh(*backend, resource_tracker, mesh_handle, mesh, 0U);
        if (!mesh_data.has_value())
            return Result(false, "Resource uploader failed: runtime mesh upload failed.");

        out_mesh = *mesh_data;
        return {};
    }

    Result ResourceUploader::upload_static_runtime_mesh(
        const Handle& mesh_handle,
        const Mesh& mesh,
        RenderingResourceTracker& resource_tracker,
        RenderingMeshUploadData& out_mesh) const
    {
        if (const auto cached_mesh = _caches.meshes.runtime_meshes.find(mesh_handle);
            cached_mesh != _caches.meshes.runtime_meshes.end())
        {
            resource_tracker.track(cached_mesh->second.vertex_buffer);
            resource_tracker.track(cached_mesh->second.index_buffer);
            out_mesh = cached_mesh->second;
            return {};
        }

        const Result result = upload_runtime_mesh(mesh_handle, mesh, resource_tracker, out_mesh);
        if (!result)
            return result;

        _caches.meshes.runtime_meshes[mesh_handle] = out_mesh;
        return {};
    }

    GraphicsResourceBinding ResourceUploader::upload_instance_buffer(
        RenderingResourceTracker& resource_tracker,
        const std::string& cache_key,
        const uint64 frame_index,
        const void* data,
        const uint64 byte_size) const
    {
        (void)frame_index;
        const auto backend = _backend.lock();
        if (!backend)
            return GraphicsResourceBinding {.slot = VERTEX_BUFFER_SLOT_INSTANCE};

        return detail::upload_instance_vertex_buffer(
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

    GraphicsResourceBinding ResourceUploader::upload_uniform_buffer(
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

        return detail::upload_or_update_uniform_buffer(
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

    GraphicsResourceBinding ResourceUploader::upload_texture(
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
        const Result result = backend->upload_texture(desc, nullptr, 0U, resource);
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

    void ResourceUploader::discard_cached_resource(const Uuid& resource)
    {
        if (!resource.is_valid())
            return;

        detail::erase_uuid_cache_entry(_caches.pipelines.pipelines, resource);
        detail::discard_cached_mesh_resource(_caches.meshes, resource);
        detail::erase_uuid_cache_entry(_caches.textures.textures, resource);
        detail::erase_uuid_cache_entry(_caches.textures.default_textures, resource);
        detail::erase_uuid_cache_entry(_caches.textures.render_targets, resource);
        detail::discard_cached_uniform_resource(_caches.uniforms, resource);
        detail::discard_cached_uniform_resource(_caches.instances, resource);
    }
}
