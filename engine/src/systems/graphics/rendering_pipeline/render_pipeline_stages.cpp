#include "render_pipeline_stages.h"

#include "render_pipeline_helpers.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/types/assets/model.h"
#include "tbx/types/assets/texture.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/mesh_bounds.h"
#include "tbx/types/vertex.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tbx
{
    //// INTERNAL SHARED HELPERS ////

    static uint64 cache_key(const ResourceCacheKey key)
    {
        return static_cast<uint64>(key);
    }

    static uint64 cache_key(const Handle& handle)
    {
        auto value = hash(handle.id);
        value = hash(handle.name, value);
        return value;
    }

    static void warn_texture_fallback_once(
        auto& state,
        const Handle& handle,
        const uint64 reason_key,
        const std::string& message)
    {
        const auto warning_key = hash(reason_key, cache_key(handle));
        if (!state.texture_fallback_warnings.emplace(warning_key).second)
            return;

        TBX_TRACE_WARNING("{}", message);
    }

    static uint64 byte_size(const uint64 value_count, const uint64 value_size)
    {
        return std::max<uint64>(value_count * value_size, value_size);
    }

    static uint32 sanitize_shadow_map_resolution(const GraphicsSettings& settings)
    {
        return settings.shadow_map_resolution == 0U ? SHADOW_ATLAS_SIZE
                                                    : settings.shadow_map_resolution;
    }

    static float sanitize_shadow_softness(const GraphicsSettings& settings)
    {
        return std::max(settings.shadow_softness, 0.0F);
    }

    template <typename TValue>
    static uint64 byte_size(const std::vector<TValue>& values)
    {
        return byte_size(static_cast<uint64>(values.size()), static_cast<uint64>(sizeof(TValue)));
    }

    static bool is_valid_gpu_id(const GpuId gpu_id)
    {
        return gpu_id != INVALID_GPU_ID;
    }

    static BufferRecord& global_material_buffer(auto& state)
    {
        return state.gpu_buffers[cache_key(ResourceCacheKey::GLOBAL_MATERIALS)];
    }

    static BufferRecord& global_mesh_buffer(auto& state)
    {
        return state.gpu_buffers[cache_key(ResourceCacheKey::GLOBAL_MESHES)];
    }

    static std::vector<ShaderMaterialData>& global_material_data(auto& state)
    {
        return global_material_buffer(state).materials;
    }

    static std::vector<MeshCacheRecord>& global_mesh_records(auto& state)
    {
        return global_mesh_buffer(state).meshes;
    }

    static std::vector<ResourceBinding> collect_texture_bindings(const auto& state)
    {
        auto bindings = std::vector<ResourceBinding>();
        bindings.reserve(state.gpu_textures.size());
        for (const auto& [_, record] : state.gpu_textures)
        {
            if (record.binding.resource_handle == INVALID_GPU_ID)
                continue;

            bindings.push_back(record.binding);
        }

        std::sort(
            bindings.begin(),
            bindings.end(),
            [](const ResourceBinding& lhs, const ResourceBinding& rhs)
            {
                return lhs.binding_slot < rhs.binding_slot;
            });
        return bindings;
    }

    static void reset_frame_build_resources(FrameBuildResources& frame)
    {
        frame.lights.clear();
        frame.instances.clear();
        frame.dynamic_mesh_sources.clear();
        frame.static_mesh_sources.clear();
        frame.view_projection = Mat4(1.0F);
        frame.camera_position = Vec3(0.0F);
        frame.sky_color = Vec4(0.55F, 0.72F, 0.9F, 1.0F);
    }

    static Result present_failure_frame(IGraphicsBackend& backend)
    {
        const auto begin_result = backend.begin_render_pass(
            RenderPassDesc {
                .clear_color = Color::MAGENTA,
                .clear_flags = GraphicsClearFlags::COLOR,
                .debug_name = "Toybox Failure Pass",
            });
        const auto pass_result = begin_result ? backend.end_render_pass() : begin_result;
        const auto present_result = backend.present();
        const auto end_result = backend.end_frame();

        if (!pass_result)
            return pass_result;
        if (!present_result)
            return present_result;
        return end_result;
    }

    //// INTERNAL HASH HELPERS ////

    static uint64 hash_shader_program(const ShaderProgram& shader)
    {
        auto value = cache_key(shader.vertex);
        value = hash(shader.fragment.id, value);
        value = hash(shader.tesselation.id, value);
        value = hash(shader.geometry.id, value);
        value = hash(static_cast<uint64>(shader.computes.size()), value);
        for (const auto& compute : shader.computes)
        {
            value = hash(compute.id, value);
        }
        return value;
    }

    static uint64 hash_material_parameter_data(
        const MaterialParameterData& data,
        uint64 value = TBX_FNV1A_OFFSET_BASIS)
    {
        value = hash(static_cast<uint64>(data.index()), value);
        std::visit(
            [&](const auto& parameter_value)
            {
                value = hash(parameter_value, value);
            },
            data);
        return value;
    }

    static uint64 hash_material_instance_key(const Handle& handle, const MaterialInstance& instance)
    {
        auto value = cache_key(handle);
        value = hash(instance.overrides.has_config_override, value);
        if (instance.overrides.has_config_override)
            value = hash(instance.overrides.config, value);

        value = hash(instance.overrides.has_parameter_override, value);
        if (instance.overrides.has_parameter_override)
        {
            value = hash(static_cast<uint64>(instance.overrides.parameters.values.size()), value);
            for (const auto& parameter : instance.overrides.parameters.values)
            {
                value = hash(parameter.id, value);
                value = hash_material_parameter_data(parameter.data, value);
            }
        }

        value = hash(instance.overrides.has_texture_override, value);
        if (instance.overrides.has_texture_override)
        {
            value = hash(static_cast<uint64>(instance.overrides.textures.values.size()), value);
            for (const auto& texture : instance.overrides.textures.values)
            {
                value = hash(texture.id, value);
                value = hash(texture.texture.id, value);
                value = hash(texture.texture.name, value);
            }
        }

        return value;
    }

    //// INTERNAL MATERIAL HELPERS ////

    static uint32 make_material_flags(const MaterialConfig& config)
    {
        uint32 flags = config.blend_mode == MaterialBlendMode::OPAQUE
                           ? SHADER_PIPELINE_FLAG_OPAQUE
                           : SHADER_PIPELINE_FLAG_TRANSPARENT;
        if (config.shadow_mode == ShadowMode::ON)
            flags |= SHADER_PIPELINE_FLAG_SHADOW;
        return flags;
    }

    static MaterialConfig resolve_material_config(
        const Material& material,
        const MaterialInstance& instance)
    {
        return instance.overrides.has_config_override ? instance.overrides.config : material.config;
    }

    static uint64 hash_renderable_pipeline_key(const ShaderProgram& shader, const MaterialConfig& config)
    {
        return hash(config, hash_shader_program(shader));
    }

    static std::optional<std::reference_wrapper<const MaterialParameter>> find_effective_parameter(
        const Material& material,
        const MaterialInstance& instance,
        std::string_view name)
    {
        if (instance.overrides.has_parameter_override)
        {
            const auto parameter = instance.overrides.parameters.get(name);
            if (parameter.has_value())
                return std::cref(parameter->get());
        }

        const auto parameter = material.parameters.get(name);
        if (parameter.has_value())
            return std::cref(parameter->get());

        return std::nullopt;
    }

    static std::optional<std::reference_wrapper<const MaterialTextureBinding>> find_effective_texture(
        const Material& material,
        const MaterialInstance& instance,
        std::string_view name)
    {
        if (instance.overrides.has_texture_override)
        {
            const auto texture = instance.overrides.textures.get(name);
            if (texture.has_value())
                return std::cref(texture->get());
        }

        const auto texture = material.textures.get(name);
        if (texture.has_value())
            return std::cref(texture->get());

        return std::nullopt;
    }

    static bool matches_material_parameter_schema(
        const MaterialParameterData& expected,
        const MaterialParameterData& actual)
    {
        return expected.index() == actual.index();
    }

    static bool try_read_scalar_parameter(
        const MaterialParameterData& data,
        float& out_value)
    {
        if (const auto* value = std::get_if<float>(&data))
        {
            out_value = *value;
            return true;
        }
        if (const auto* value = std::get_if<double>(&data))
        {
            out_value = static_cast<float>(*value);
            return true;
        }
        if (const auto* value = std::get_if<int>(&data))
        {
            out_value = static_cast<float>(*value);
            return true;
        }

        return false;
    }

    static bool is_declared_material_parameter(
        const Material& material,
        std::string_view name)
    {
        return std::ranges::any_of(
            material.parameters.values,
            [name](const MaterialParameter& parameter)
            {
                return parameter.name == name;
            });
    }

    static bool is_declared_material_texture(
        const Material& material,
        std::string_view name)
    {
        return std::ranges::any_of(
            material.textures.values,
            [name](const MaterialTextureBinding& texture)
            {
                return texture.name == name;
            });
    }

    static void warn_material_contract_once(
        auto& state,
        const Handle& material_handle,
        std::string_view binding_name,
        const uint64 reason_key,
        const std::string& message)
    {
        auto warning_key = hash(reason_key, cache_key(material_handle));
        warning_key = hash(binding_name, warning_key);
        if (!state.material_contract_warnings.emplace(warning_key).second)
            return;

        TBX_TRACE_WARNING("{}", message);
    }

    static void warn_unused_material_bindings(
        auto& state,
        const Handle& material_handle,
        const Material& material,
        const MaterialInstance& instance)
    {
        if (instance.overrides.has_parameter_override)
        {
            for (const auto& parameter : instance.overrides.parameters.values)
            {
                if (parameter.name.empty()
                    || is_declared_material_parameter(material, parameter.name))
                {
                    continue;
                }

                warn_material_contract_once(
                    state,
                    material_handle,
                    parameter.name,
                    1U,
                    std::format(
                        "Rendering pipeline: material override '{}' on '{}' is undeclared; it will not be uploaded.",
                        parameter.name,
                        material_handle.name));
            }
        }

        if (instance.overrides.has_texture_override)
        {
            for (const auto& texture : instance.overrides.textures.values)
            {
                if (texture.name.empty()
                    || is_declared_material_texture(material, texture.name))
                {
                    continue;
                }

                warn_material_contract_once(
                    state,
                    material_handle,
                    texture.name,
                    2U,
                    std::format(
                        "Rendering pipeline: material override texture '{}' on '{}' is undeclared; it will not be uploaded.",
                        texture.name,
                        material_handle.name));
            }
        }
    }

    static Result validate_renderable_raster_shader(const Material& material)
    {
        if (!material.shader.is_valid())
        {
            return make_render_pipeline_failure(
                "Rendering pipeline: frame material is missing a valid raster shader program.");
        }
        return Result(true);
    }

    //// INTERNAL TEXTURE HELPERS ////

    static RenderTexture make_runtime_texture(
        const Size& resolution,
        const TextureFormat format,
        const TextureUsage usage)
    {
        return RenderTexture(
            resolution,
            TextureWrap::REPEAT,
            TextureFilter::LINEAR,
            format,
            TextureMipmaps::DISABLED,
            TextureCompression::DISABLED,
            usage);
    }

    static GraphicsTextureDesc make_texture_desc(
        const RenderTexture& texture,
        const std::string_view debug_name,
        const uint32 array_layer_count = 1U,
        const bool is_depth_comparison_enabled = false)
    {
        return GraphicsTextureDesc {
            .usage = texture.usage,
            .format = texture.format,
            .size = texture.resolution,
            .array_layer_count = array_layer_count,
            .is_depth_comparison_enabled = is_depth_comparison_enabled,
            .debug_name = std::string(debug_name),
        };
    }

    static std::pair<const Pixel*, uint64> get_texture_upload_data(const Texture& texture)
    {
        static constexpr std::array<Pixel, 3U> WHITE_RGB = {255U, 255U, 255U};
        static constexpr std::array<Pixel, 4U> WHITE_RGBA = {255U, 255U, 255U, 255U};

        if (!texture.pixels.empty())
            return {texture.pixels.data(), byte_size(texture.pixels)};

        if (texture.format == TextureFormat::RGB)
            return {WHITE_RGB.data(), static_cast<uint64>(WHITE_RGB.size())};

        return {WHITE_RGBA.data(), static_cast<uint64>(WHITE_RGBA.size())};
    }

    static Texture make_semantic_default_texture(const uint32 slot)
    {
        switch (slot)
        {
            case DEFAULT_ALBEDO_TEXTURE_SLOT:
            case DEFAULT_SCALAR_TEXTURE_SLOT:
                return Texture(
                    Size {.width = 1U, .height = 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    TextureMipmaps::DISABLED,
                    TextureCompression::DISABLED,
                    std::vector<Pixel> {255U, 255U, 255U, 255U});
            case DEFAULT_NORMAL_TEXTURE_SLOT:
                return Texture(
                    Size {.width = 1U, .height = 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    TextureMipmaps::DISABLED,
                    TextureCompression::DISABLED,
                    std::vector<Pixel> {128U, 128U, 255U, 255U});
            case DEFAULT_EMISSIVE_TEXTURE_SLOT:
                return Texture(
                    Size {.width = 1U, .height = 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    TextureMipmaps::DISABLED,
                    TextureCompression::DISABLED,
                    std::vector<Pixel> {0U, 0U, 0U, 255U});
            default:
                return Texture(
                    Size {.width = 1U, .height = 1U},
                    TextureWrap::REPEAT,
                    TextureFilter::LINEAR,
                    TextureFormat::RGBA,
                    TextureMipmaps::DISABLED,
                    TextureCompression::DISABLED,
                    std::vector<Pixel> {255U, 0U, 255U, 255U});
        }
    }

    static Vec3 sample_texture_rgb(const Texture& texture, const float u, const float v)
    {
        if (texture.pixels.empty() || texture.resolution.width == 0U
            || texture.resolution.height == 0U)
            return Vec3(1.0F);

        const uint32 channel_count = texture.format == TextureFormat::RGB ? 3U : 4U;
        const float wrapped_u = u - std::floor(u);
        const float wrapped_v = v - std::floor(v);
        const uint32 x = std::min(
            static_cast<uint32>(wrapped_u * static_cast<float>(texture.resolution.width)),
            texture.resolution.width - 1U);
        const uint32 y = std::min(
            static_cast<uint32>(wrapped_v * static_cast<float>(texture.resolution.height)),
            texture.resolution.height - 1U);
        const size pixel_index = (static_cast<size>(y) * static_cast<size>(texture.resolution.width)
                                  + static_cast<size>(x))
                                 * static_cast<size>(channel_count);
        if (pixel_index + 2U >= texture.pixels.size())
            return Vec3(1.0F);

        return Vec3(
            static_cast<float>(texture.pixels[pixel_index]) / 255.0F,
            static_cast<float>(texture.pixels[pixel_index + 1U]) / 255.0F,
            static_cast<float>(texture.pixels[pixel_index + 2U]) / 255.0F);
    }

    static Vec4 approximate_sky_texture_tint(const Texture& texture)
    {
        static constexpr std::array<Vec2, 5U> SAMPLE_POINTS = {
            Vec2(0.25F, 0.20F),
            Vec2(0.50F, 0.18F),
            Vec2(0.75F, 0.20F),
            Vec2(0.35F, 0.32F),
            Vec2(0.65F, 0.32F),
        };

        auto tint = Vec3(0.0F);
        for (const auto& sample : SAMPLE_POINTS)
            tint += sample_texture_rgb(texture, sample.x, sample.y);

        tint /= static_cast<float>(SAMPLE_POINTS.size());
        return Vec4(tint, 1.0F);
    }

    static std::string_view get_semantic_default_texture_name(const uint32 slot)
    {
        switch (slot)
        {
            case DEFAULT_ALBEDO_TEXTURE_SLOT:
                return "ToyboxDefaultAlbedo";
            case DEFAULT_NORMAL_TEXTURE_SLOT:
                return "ToyboxDefaultNormal";
            case DEFAULT_SCALAR_TEXTURE_SLOT:
                return "ToyboxDefaultScalar";
            case DEFAULT_EMISSIVE_TEXTURE_SLOT:
                return "ToyboxDefaultEmissive";
            default:
                return "ToyboxDefaultFallback";
        }
    }

    //// INTERNAL RESOURCE CACHE ////

    static Result create_or_update_buffer(
        IGraphicsBackend& backend,
        auto& state,
        const uint64 key,
        const std::string_view debug_name,
        const GraphicsBufferUsage usage,
        const uint64 required_size,
        const void* data,
        const uint64 data_size,
        GpuId& out_resource)
    {
        const uint64 capacity = std::max(required_size, data_size);
        if (capacity == 0U)
            return make_render_pipeline_failure("Rendering pipeline: buffer size must be greater than zero.");

        auto record = state.gpu_buffers.find(key);
        if (record == state.gpu_buffers.end() || record->second.desc.size < capacity
            || record->second.desc.usage != usage)
        {
            auto material_data = std::vector<ShaderMaterialData>();
            auto mesh_records = std::vector<MeshCacheRecord>();
            if (record != state.gpu_buffers.end())
            {
                material_data = std::move(record->second.materials);
                mesh_records = std::move(record->second.meshes);
            }

            if (record != state.gpu_buffers.end() && is_valid_gpu_id(record->second.resource))
                backend.destroy_resource(record->second.resource);

            auto resource = INVALID_GPU_ID;
            const auto desc = GraphicsBufferDesc {
                .usage = usage,
                .size = capacity,
                .is_dynamic = true,
                .debug_name = std::string(debug_name),
            };
            if (auto result = backend.create_buffer(desc, resource); !result)
                return warn_render_pipeline_backend_failure(
                    std::format("failed to create buffer '{}'", debug_name),
                    result);

            record = state.gpu_buffers
                         .insert_or_assign(
                             key,
                             BufferRecord {
                                 .resource = resource,
                                 .desc = desc,
                                 .materials = std::move(material_data),
                                 .meshes = std::move(mesh_records),
                             })
                         .first;
        }

        out_resource = record->second.resource;
        if (data_size == 0U)
            return Result(true);

        return warn_render_pipeline_backend_failure(
            std::format("failed to upload buffer '{}'", debug_name),
            backend.write_buffer(out_resource, data, data_size, 0U));
    }

    template <typename TValue>
    static Result upload_vector_buffer(
        IGraphicsBackend& backend,
        auto& state,
        const uint64 key,
        const std::string_view debug_name,
        const GraphicsBufferUsage usage,
        const std::vector<TValue>& values,
        GpuId& out_resource)
    {
        const TValue fallback = {};
        const void* data = values.empty() ? &fallback : values.data();
        const uint64 data_size =
            values.empty() ? static_cast<uint64>(sizeof(TValue)) : byte_size(values);
        return create_or_update_buffer(
            backend,
            state,
            key,
            debug_name,
            usage,
            data_size,
            data,
            data_size,
            out_resource);
    }

    static Result create_or_update_texture(
        IGraphicsBackend& backend,
        auto& state,
        const uint64 key,
        const RenderTexture& texture,
        const std::string_view debug_name,
        const uint32 array_layer_count,
        const bool is_depth_comparison_enabled,
        GpuId& out_resource,
        bool& out_needs_upload,
        TextureRecord** out_record = nullptr)
    {
        const auto desc =
            make_texture_desc(texture, debug_name, array_layer_count, is_depth_comparison_enabled);
        auto record = state.gpu_textures.find(key);
        const bool needs_recreate =
            record == state.gpu_textures.end() || record->second.desc.usage != desc.usage
            || record->second.desc.format != desc.format
            || record->second.desc.size.width != desc.size.width
            || record->second.desc.size.height != desc.size.height
            || record->second.desc.array_layer_count != desc.array_layer_count;
        out_needs_upload = needs_recreate;

        if (needs_recreate)
        {
            const auto binding =
                record != state.gpu_textures.end() ? record->second.binding : ResourceBinding {};
            if (record != state.gpu_textures.end() && is_valid_gpu_id(record->second.resource))
                backend.destroy_resource(record->second.resource);

            auto resource = INVALID_GPU_ID;
            if (auto result = backend.create_texture(desc, resource); !result)
                return warn_render_pipeline_backend_failure(
                    std::format("failed to create texture '{}'", debug_name),
                    result);

            record = state.gpu_textures
                         .insert_or_assign(
                             key,
                             TextureRecord {
                                 .resource = resource,
                                 .desc = desc,
                                 .binding = binding,
                             })
                         .first;
        }

        out_resource = record->second.resource;
        if (out_record != nullptr)
            *out_record = &record->second;

        return Result(true);
    }

    static Result upload_texture(
        IGraphicsBackend& backend,
        auto& state,
        const uint64 key,
        const Handle& handle,
        const Texture& texture,
        const GpuId gpu_id = INVALID_GPU_ID)
    {
        auto resource = INVALID_GPU_ID;
        bool needs_upload = false;
        const auto runtime_texture =
            make_runtime_texture(texture.resolution, texture.format, TextureUsage::SAMPLED);
        auto* record = static_cast<TextureRecord*>(nullptr);
        if (auto result = create_or_update_texture(
                backend,
                state,
                key,
                runtime_texture,
                handle.name,
                1U,
                false,
                resource,
                needs_upload,
                &record);
            !result)
        {
            return result;
        }

        if (record == nullptr)
            return make_render_pipeline_failure("Rendering pipeline: missing cached texture record.");
        if (gpu_id != INVALID_GPU_ID)
        {
            record->binding = ResourceBinding {
                .binding_slot = static_cast<uint32>(SHADER_BINDING_GLOBAL_TEXTURES + gpu_id),
                .resource_handle = resource,
            };
        }
        if (!needs_upload)
            return Result(true);

        const auto update = GraphicsTextureUpdateDesc {
            .width = texture.resolution.width,
            .height = texture.resolution.height,
        };
        const auto [pixel_data, pixel_data_size] = get_texture_upload_data(texture);
        if (auto result = backend.write_texture(resource, update, pixel_data, pixel_data_size);
            !result)
        {
            return warn_render_pipeline_backend_failure(
                std::format("failed to upload texture '{}'", handle.name),
                result);
        }

        return Result(true);
    }

    static Result upload_fallback_textures(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets)
    {
        for (uint32 slot = 0U; slot < FIRST_MATERIAL_TEXTURE_SLOT; ++slot)
        {
            const auto texture = make_semantic_default_texture(slot);
            if (auto result = upload_texture(
                    backend,
                    state,
                    hash(static_cast<uint64>(slot), cache_key(ResourceCacheKey::FINAL_HDR)),
                    Handle(std::string(get_semantic_default_texture_name(slot))),
                    texture,
                    slot);
                !result)
            {
                return result;
            }
        }

        return Result(true);
    }

    static GpuId cache_material_texture(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const Handle& handle,
        const GpuId fallback_slot)
    {
        if (!handle.id.is_valid())
            return fallback_slot;

        if (const auto slot = state.texture_lookup.find(handle); slot != state.texture_lookup.end())
            return slot->second;

        const GpuId slot =
            FIRST_MATERIAL_TEXTURE_SLOT + static_cast<GpuId>(state.texture_lookup.size());
        if (slot >= MAX_GLOBAL_TEXTURES)
        {
            warn_texture_fallback_once(
                state,
                handle,
                1U,
                std::format(
                    "Rendering pipeline: texture '{}' exceeded the global texture budget; using "
                    "fallback slot {}.",
                    handle.name,
                    fallback_slot));
            return fallback_slot;
        }

        auto texture = assets.load<Texture>(handle);
        if (!texture)
        {
            warn_texture_fallback_once(
                state,
                handle,
                2U,
                std::format(
                    "Rendering pipeline: failed to load texture '{}'; using semantic fallback "
                    "slot {}.",
                    handle.name,
                    fallback_slot));
            return fallback_slot;
        }

        const uint64 key = cache_key(handle);
        if (auto result = upload_texture(backend, state, key, handle, *texture, slot); !result)
        {
            warn_texture_fallback_once(
                state,
                handle,
                3U,
                std::format(
                    "Rendering pipeline: failed to upload texture '{}'; using fallback slot {}. "
                    "{}",
                    handle.name,
                    fallback_slot,
                    result.get_report()));
            return fallback_slot;
        }

        state.texture_lookup[handle] = slot;
        return slot;
    }

    static bool try_apply_shader_material_parameter(
        ShaderMaterialData& out_data,
        const MaterialBindingTarget target,
        const MaterialParameterData& data)
    {
        switch (target)
        {
            case MaterialBindingTarget::BASE_COLOR:
            {
                const auto* color = std::get_if<Color>(&data);
                if (color == nullptr)
                    return false;

                out_data.base_color = Vec4(color->r, color->g, color->b, color->a);
                return true;
            }
            case MaterialBindingTarget::EMISSIVE_COLOR:
            {
                const auto* color = std::get_if<Color>(&data);
                if (color == nullptr)
                    return false;

                out_data.emissive_color = Vec4(color->r, color->g, color->b, color->a);
                return true;
            }
            case MaterialBindingTarget::METALLIC:
                return try_read_scalar_parameter(data, out_data.surface.x);
            case MaterialBindingTarget::ROUGHNESS:
                return try_read_scalar_parameter(data, out_data.surface.y);
            case MaterialBindingTarget::NORMAL_STRENGTH:
                return try_read_scalar_parameter(data, out_data.surface.z);
            case MaterialBindingTarget::AO:
                return try_read_scalar_parameter(data, out_data.surface.w);
            case MaterialBindingTarget::ALPHA_CUTOFF:
                return try_read_scalar_parameter(data, out_data.alpha_cutoff);
            default:
                return false;
        }
    }

    static bool try_apply_shader_material_texture(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        ShaderMaterialData& out_data,
        const MaterialBindingTarget target,
        const Handle& texture)
    {
        switch (target)
        {
            case MaterialBindingTarget::ALBEDO_TEXTURE:
                out_data.albedo_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_ALBEDO_TEXTURE_SLOT);
                return true;
            case MaterialBindingTarget::NORMAL_TEXTURE:
                out_data.normal_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_NORMAL_TEXTURE_SLOT);
                return true;
            case MaterialBindingTarget::METALLIC_TEXTURE:
                out_data.metallic_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_SCALAR_TEXTURE_SLOT);
                return true;
            case MaterialBindingTarget::ROUGHNESS_TEXTURE:
                out_data.roughness_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_SCALAR_TEXTURE_SLOT);
                return true;
            case MaterialBindingTarget::AO_TEXTURE:
                out_data.ao_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_ALBEDO_TEXTURE_SLOT);
                return true;
            case MaterialBindingTarget::EMISSIVE_TEXTURE:
                out_data.emissive_texture_index = cache_material_texture(
                    backend,
                    state,
                    assets,
                    texture,
                    DEFAULT_EMISSIVE_TEXTURE_SLOT);
                return true;
            default:
                return false;
        }
    }

    static std::optional<ShaderMaterialData> try_make_shader_material_from_contract(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const Handle& material_handle,
        const Material& material,
        const MaterialInstance& instance)
    {
        const auto config = resolve_material_config(material, instance);
        auto data = ShaderMaterialData {
            .pipeline_flags = make_material_flags(config),
            .albedo_texture_index = DEFAULT_ALBEDO_TEXTURE_SLOT,
            .normal_texture_index = DEFAULT_NORMAL_TEXTURE_SLOT,
            .metallic_texture_index = DEFAULT_SCALAR_TEXTURE_SLOT,
            .roughness_texture_index = DEFAULT_SCALAR_TEXTURE_SLOT,
            .ao_texture_index = DEFAULT_ALBEDO_TEXTURE_SLOT,
            .emissive_texture_index = DEFAULT_EMISSIVE_TEXTURE_SLOT,
            .alpha_cutoff = 0.0F,
            .base_color = Vec4(Color::WHITE.r, Color::WHITE.g, Color::WHITE.b, Color::WHITE.a),
            .emissive_color = Vec4(Color::BLACK.r, Color::BLACK.g, Color::BLACK.b, Color::BLACK.a),
            .surface = Vec4(0.0F, 1.0F, 1.0F, 1.0F),
        };

        warn_unused_material_bindings(state, material_handle, material, instance);
        for (const auto& parameter_schema : material.parameters.values)
        {
            if (parameter_schema.target == MaterialBindingTarget::NONE)
            {
                continue;
            }

            const auto parameter = find_effective_parameter(material, instance, parameter_schema.name);
            if (!parameter.has_value())
            {
                warn_material_contract_once(
                    state,
                    material_handle,
                    parameter_schema.name,
                    3U,
                    std::format(
                        "Rendering pipeline: material '{}' is missing declared parameter '{}'.",
                        material_handle.name,
                        parameter_schema.name));
                return std::nullopt;
            }

            if (!matches_material_parameter_schema(parameter_schema.data, parameter->get().data)
                || !try_apply_shader_material_parameter(
                    data,
                    parameter_schema.target,
                    parameter->get().data))
            {
                warn_material_contract_once(
                    state,
                    material_handle,
                    parameter_schema.name,
                    4U,
                    std::format(
                        "Rendering pipeline: material '{}' parameter '{}' does not match the declared material schema.",
                        material_handle.name,
                        parameter_schema.name));
                return std::nullopt;
            }
        }

        for (const auto& texture_schema : material.textures.values)
        {
            if (texture_schema.target == MaterialBindingTarget::NONE)
                continue;

            const auto texture = find_effective_texture(material, instance, texture_schema.name);
            if (!texture.has_value())
            {
                warn_material_contract_once(
                    state,
                    material_handle,
                    texture_schema.name,
                    5U,
                    std::format(
                        "Rendering pipeline: material '{}' is missing declared texture '{}'.",
                        material_handle.name,
                        texture_schema.name));
                return std::nullopt;
            }

            if (texture->get().texture.id.is_valid()
                && !try_apply_shader_material_texture(
                    backend,
                    state,
                    assets,
                    data,
                    texture_schema.target,
                    texture->get().texture))
            {
                warn_material_contract_once(
                    state,
                    material_handle,
                    texture_schema.name,
                    6U,
                    std::format(
                        "Rendering pipeline: material '{}' texture '{}' could not be uploaded to the declared material target.",
                        material_handle.name,
                        texture_schema.name));
                return std::nullopt;
            }
        }

        return data;
    }

    static ShaderMaterialData make_magenta_fallback_material_data()
    {
        return ShaderMaterialData {
            .pipeline_flags = SHADER_PIPELINE_FLAG_OPAQUE,
            .albedo_texture_index = DEFAULT_ALBEDO_TEXTURE_SLOT,
            .normal_texture_index = DEFAULT_NORMAL_TEXTURE_SLOT,
            .metallic_texture_index = DEFAULT_SCALAR_TEXTURE_SLOT,
            .roughness_texture_index = DEFAULT_SCALAR_TEXTURE_SLOT,
            .ao_texture_index = DEFAULT_ALBEDO_TEXTURE_SLOT,
            .emissive_texture_index = DEFAULT_EMISSIVE_TEXTURE_SLOT,
            .alpha_cutoff = 0.0F,
            .base_color = Vec4(1.0F, 0.0F, 1.0F, 1.0F),
            .emissive_color = Vec4(1.0F, 0.0F, 1.0F, 1.0F),
            .surface = Vec4(0.0F, 1.0F, 1.0F, 1.0F),
        };
    }

    static ShaderMaterialData make_shader_material(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const Handle& material_handle,
        const Material& material,
        const MaterialInstance& instance)
    {
        if (const auto data =
                try_make_shader_material_from_contract(
                    backend,
                    state,
                    assets,
                    material_handle,
                    material,
                    instance);
            data.has_value())
        {
            return *data;
        }

        if (material_handle != FALLBACK_MATERIAL_HANDLE)
        {
            auto fallback = assets.load<Material>(FALLBACK_MATERIAL_HANDLE);
            if (fallback)
            {
                warn_material_contract_once(
                    state,
                    material_handle,
                    "fallback",
                    7U,
                    std::format(
                        "Rendering pipeline: material '{}' failed material contract validation; using fallback material '{}'.",
                        material_handle.name,
                        FALLBACK_MATERIAL_HANDLE.name));
                return make_shader_material(
                    backend,
                    state,
                    assets,
                    FALLBACK_MATERIAL_HANDLE,
                    *fallback,
                    MaterialInstance(FALLBACK_MATERIAL_HANDLE));
            }
        }

        warn_material_contract_once(
            state,
            material_handle,
            "fallback",
            8U,
            std::format(
                "Rendering pipeline: fallback material '{}' is unavailable or invalid; using built-in magenta constants.",
                FALLBACK_MATERIAL_HANDLE.name));
        return make_magenta_fallback_material_data();
    }

    static std::vector<Shader> load_compute_shaders(AssetManager& assets, const Handle& handle)
    {
        auto shaders = std::vector<Shader>();
        const auto shader = assets.load<Shader>(handle);
        if (shader)
            shaders.push_back(*shader);
        return shaders;
    }

    static std::vector<Shader> load_raster_shaders(
        AssetManager& assets,
        const ShaderProgram& shader)
    {
        auto shaders = std::vector<Shader>();
        shaders.reserve(2U);

        const auto vertex = assets.load<Shader>(shader.vertex);
        const auto fragment = assets.load<Shader>(shader.fragment);
        if (vertex)
            shaders.push_back(*vertex);
        if (fragment)
            shaders.push_back(*fragment);

        return shaders;
    }

    //// INTERNAL PIPELINE CACHE ////

    static uint64 make_layout_key(const BindGroupLayoutDesc& desc)
    {
        auto value = hash(desc.debug_name);
        for (const auto& entry : desc.entries)
        {
            value = hash(entry.binding_slot, value);
            value = hash(static_cast<uint32>(entry.type), value);
            value = hash(entry.shader_stages, value);
        }
        return value;
    }

    static Result get_bind_group_layout(
        IGraphicsBackend& backend,
        auto& state,
        const BindGroupLayoutDesc& desc,
        GpuId& out_resource)
    {
        const auto key = make_layout_key(desc);
        if (const auto cached = state.gpu_bind_group_layouts.find(key);
            cached != state.gpu_bind_group_layouts.end())
        {
            out_resource = cached->second.resource;
            return Result(true);
        }

        if (auto result = backend.create_bind_group_layout(desc, out_resource); !result)
            return result;

        state.gpu_bind_group_layouts[key] = ResourceRecord {.resource = out_resource};
        return Result(true);
    }

    static uint64 make_bind_group_key(const BindGroupDesc& desc)
    {
        auto value = hash(desc.layout_handle);
        value = hash(desc.debug_name, value);
        for (const auto& binding : desc.bindings)
        {
            value = hash(binding.binding_slot, value);
            value = hash(binding.resource_handle, value);
            value = hash(binding.offset, value);
            value = hash(binding.range, value);
        }
        return value;
    }

    static Result bind_group(
        IGraphicsBackend& backend,
        auto& state,
        const uint32 set_index,
        const BindGroupDesc& desc)
    {
        const auto key = make_bind_group_key(desc);
        auto group = INVALID_GPU_ID;
        if (const auto cached = state.gpu_bind_groups.find(key);
            cached != state.gpu_bind_groups.end())
        {
            group = cached->second.resource;
        }
        else
        {
            if (auto result = backend.create_bind_group(desc, group); !result)
                return result;
            state.gpu_bind_groups[key] = ResourceRecord {.resource = group};
        }

        return backend.bind_group(set_index, group);
    }

    static Result get_compute_pipeline(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const uint64 key,
        const std::string_view debug_name,
        const Handle& shader_handle,
        GpuId& out_resource)
    {
        if (const auto cached = state.gpu_compute_pipelines.find(key);
            cached != state.gpu_compute_pipelines.end())
        {
            out_resource = cached->second.resource;
            return Result(true);
        }

        const auto shaders = load_compute_shaders(assets, shader_handle);
        if (shaders.size() != 1U)
            return make_render_pipeline_failure(
                std::format("Rendering pipeline: missing shader '{}'.", shader_handle.name));

        const auto desc = ComputePipelineDesc {
            .shaders = shaders,
            .debug_name = std::string(debug_name),
        };
        if (auto result = backend.create_compute_pipeline(desc, out_resource); !result)
            return result;

        state.gpu_compute_pipelines[key] = ResourceRecord {.resource = out_resource};
        return Result(true);
    }

    static Result get_raster_pipeline(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const uint64 key,
        const std::string_view debug_name,
        const ShaderProgram& shader,
        const RasterPipelineDesc& pipeline_desc,
        GpuId& out_resource)
    {
        if (const auto cached = state.gpu_raster_pipelines.find(key);
            cached != state.gpu_raster_pipelines.end())
        {
            out_resource = cached->second.resource;
            return Result(true);
        }

        auto desc = pipeline_desc;
        desc.shaders = load_raster_shaders(assets, shader);
        desc.debug_name = std::string(debug_name);
        if (!shader.is_valid() || desc.shaders.size() != 2U)
            return make_render_pipeline_failure(
                std::format("Rendering pipeline: missing raster shader for '{}'.", debug_name));

        if (auto result = backend.create_raster_pipeline(desc, out_resource); !result)
            return result;

        state.gpu_raster_pipelines[key] = ResourceRecord {.resource = out_resource};
        return Result(true);
    }

    static std::vector<ResourceBinding> make_common_bindings(const FrameBuffers& buffers)
    {
        auto bindings = std::vector<ResourceBinding>();
        bindings.reserve(13U);
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_ALL_INSTANCES,
                .resource_handle = buffers.instances,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_GLOBAL_VERTICES,
                .resource_handle = buffers.vertices,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_GLOBAL_MESHES,
                .resource_handle = buffers.meshes,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_GLOBAL_MATERIALS,
                .resource_handle = buffers.materials,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_GLOBAL_LIGHTS,
                .resource_handle = buffers.lights,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_CLUSTER_GRID,
                .resource_handle = buffers.cluster_grid,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_LIGHT_INDEX_POOL,
                .resource_handle = buffers.light_index_pool,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_MAIN_SCENE_DRAW_ARGS,
                .resource_handle = buffers.main_draw_args,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_SHADOW_ARGS_POOL,
                .resource_handle = buffers.shadow_draw_args,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_MAIN_SCENE_DRAW_COUNT,
                .resource_handle = buffers.main_draw_count,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_SHADOW_DRAW_COUNT,
                .resource_handle = buffers.shadow_draw_count,
            });
        bindings.push_back(
            ResourceBinding {
                .binding_slot = SHADER_BINDING_SCENE_UNIFORMS,
                .resource_handle = buffers.frame_uniforms,
            });
        return bindings;
    }

    static void append_binding(
        std::vector<ResourceBinding>& bindings,
        const uint32 binding_slot,
        const GpuId resource_handle)
    {
        bindings.push_back(
            ResourceBinding {
                .binding_slot = binding_slot,
                .resource_handle = resource_handle,
            });
    }

    static void append_gbuffer_bindings(
        std::vector<ResourceBinding>& bindings,
        const FrameBuffers& buffers)
    {
        append_binding(bindings, SHADER_BINDING_GBUFFER_ALBEDO, buffers.gbuffer_albedo);
        append_binding(bindings, SHADER_BINDING_GBUFFER_ROUGHNESS, buffers.gbuffer_roughness);
        append_binding(bindings, SHADER_BINDING_GBUFFER_NORMAL, buffers.gbuffer_normal);
        append_binding(bindings, SHADER_BINDING_GBUFFER_METALLIC, buffers.gbuffer_metallic);
        append_binding(bindings, SHADER_BINDING_SCENE_DEPTH, buffers.frame_depth);
        append_binding(bindings, SHADER_BINDING_SHADOW_DEPTH_ATLAS, buffers.shadow_atlas);
        append_binding(bindings, SHADER_BINDING_FINAL_HDR, buffers.final_hdr);
    }

    template <typename TBody>
    static Result with_compute_pass(
        IGraphicsBackend& backend,
        const GraphicsComputePassDesc& desc,
        TBody&& body)
    {
        if (auto result = backend.begin_compute_pass(desc); !result)
            return result;

        const auto result = body();
        const auto end_result = backend.end_compute_pass();
        return result ? end_result : result;
    }

    template <typename TBody>
    static Result with_render_pass(
        IGraphicsBackend& backend,
        const RenderPassDesc& desc,
        TBody&& body)
    {
        if (auto result = backend.begin_render_pass(desc); !result)
            return result;

        const auto result = body();
        const auto end_result = backend.end_render_pass();
        return result ? end_result : result;
    }

    static ShaderProgram make_present_shader_program()
    {
        return ShaderProgram {
            .vertex = PRESENT_VERTEX_SHADER_HANDLE,
            .fragment = PRESENT_FRAGMENT_SHADER_HANDLE,
        };
    }

    static RasterPipelineDesc make_gbuffer_pipeline_desc(const MaterialConfig& config)
    {
        return RasterPipelineDesc {
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .depth_function = config.depth_function,
            .is_depth_test_enabled = config.is_depth_test_enabled,
            .is_depth_write_enabled = config.is_depth_write_enabled,
            .is_blending_enabled = false,
            .is_culling_enabled = config.is_cullable && !config.is_two_sided,
            .cull_mode = GraphicsCullMode::BACK,
        };
    }

    static BindGroupLayoutDesc make_lighting_resolve_layout_desc()
    {
        return BindGroupLayoutDesc {
            .entries =
                {
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_GBUFFER_ALBEDO,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_GBUFFER_ROUGHNESS,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_GBUFFER_NORMAL,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_GBUFFER_METALLIC,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_SCENE_DEPTH,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_SHADOW_DEPTH_ATLAS,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_FINAL_HDR,
                        .type = BindingType::STORAGE_TEXTURE,
                        .shader_stages = SHADER_STAGE_COMPUTE,
                    },
                },
            .debug_name = "Toybox Lighting Resolve Layout",
        };
    }

    static BindGroupLayoutDesc make_present_layout_desc()
    {
        return BindGroupLayoutDesc {
            .entries =
                {
                    BindGroupLayoutEntry {
                        .binding_slot = SHADER_BINDING_FINAL_HDR,
                        .type = BindingType::SAMPLED_TEXTURE,
                        .shader_stages = SHADER_STAGE_FRAGMENT,
                    },
                },
            .debug_name = "Toybox Present Layout",
        };
    }

    static RasterPipelineDesc make_present_pipeline_desc()
    {
        return RasterPipelineDesc {
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .depth_function = MaterialDepthFunction::ALWAYS,
            .is_depth_test_enabled = false,
            .is_depth_write_enabled = false,
            .is_blending_enabled = false,
            .is_culling_enabled = false,
        };
    }

    static RasterPipelineDesc make_shadow_pipeline_desc()
    {
        return RasterPipelineDesc {
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .depth_function = MaterialDepthFunction::LESS,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_blending_enabled = false,
            .is_culling_enabled = true,
            .depth_bias_constant = 1.25F,
            .depth_bias_slope = 1.75F,
            .cull_mode = GraphicsCullMode::BACK,
        };
    }

    //// INTERNAL FRAME UPLOADS ////

    static Result upload_material_entry(
        IGraphicsBackend& backend,
        auto& state,
        const GpuId material_id)
    {
        auto& record = global_material_buffer(state);
        auto& materials = record.materials;
        if (material_id >= materials.size())
            return make_render_pipeline_failure("Rendering pipeline: material upload index out of range.");

        const auto key = cache_key(ResourceCacheKey::GLOBAL_MATERIALS);
        const auto existing_record = state.gpu_buffers.find(key);
        const bool had_existing_buffer = existing_record != state.gpu_buffers.end()
                                         && is_valid_gpu_id(existing_record->second.resource);
        const uint64 previous_capacity =
            had_existing_buffer ? existing_record->second.desc.size : 0U;
        auto buffer = INVALID_GPU_ID;
        if (auto result = create_or_update_buffer(
                backend,
                state,
                key,
                "global_materials",
                GraphicsBufferUsage::STORAGE,
                byte_size(materials),
                nullptr,
                0U,
                buffer);
            !result)
        {
            return result;
        }

        const auto buffer_record = state.gpu_buffers.find(key);
        const bool needs_full_upload =
            !had_existing_buffer || buffer_record->second.desc.size != previous_capacity;
        if (needs_full_upload)
        {
            if (auto result =
                    backend.write_buffer(buffer, materials.data(), byte_size(materials), 0U);
                !result)
            {
                return warn_render_pipeline_backend_failure("failed to upload global material buffer", result);
            }
        }
        else
        {
            if (auto result = backend.write_buffer(
                    buffer,
                    &materials[material_id],
                    static_cast<uint64>(sizeof(ShaderMaterialData)),
                    static_cast<uint64>(material_id)
                        * static_cast<uint64>(sizeof(ShaderMaterialData)));
                !result)
            {
                return warn_render_pipeline_backend_failure(
                    "failed to update global material buffer entry",
                    result);
            }
        }

        return Result(true);
    }

    static Result upload_cached_material_buffer(
        IGraphicsBackend& backend,
        auto& state,
        FrameBuffers& buffers)
    {
        auto& materials = global_material_data(state);
        if (materials.empty())
        {
            return upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::GLOBAL_MATERIALS),
                "global_materials",
                GraphicsBufferUsage::STORAGE,
                materials,
                buffers.materials);
        }

        const auto material_buffer =
            state.gpu_buffers.find(cache_key(ResourceCacheKey::GLOBAL_MATERIALS));
        if (material_buffer == state.gpu_buffers.end()
            || !is_valid_gpu_id(material_buffer->second.resource))
        {
            if (auto result = upload_material_entry(backend, state, 0U); !result)
                return result;
        }

        buffers.materials =
            state.gpu_buffers.at(cache_key(ResourceCacheKey::GLOBAL_MATERIALS)).resource;
        return Result(true);
    }

    static Result upload_frame_resources(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const FrameBuildResources& frame,
        const std::vector<ShaderInstanceData>& instances,
        const std::vector<ShaderLightData>& lights,
        const ShaderSceneUniforms& frame_uniforms,
        const bool geometry_changed,
        UploadedFrameResources& resources)
    {
        auto& buffers = resources.buffers;
        const bool has_cached_geometry =
            state.gpu_buffers.contains(cache_key(ResourceCacheKey::GLOBAL_VERTICES))
            && state.gpu_buffers.contains(cache_key(ResourceCacheKey::GLOBAL_INDICES))
            && state.gpu_buffers.contains(cache_key(ResourceCacheKey::GLOBAL_MESHES));
        if (
            // Rebuilding every frame forces full CPU-side mesh repacks and GPU uploads even when
            // the frame geometry is static, which dominated steady-state frame work.
            geometry_changed || !has_cached_geometry)
        {
            if (auto result = rebuild_geometry_buffers(backend, state, assets, frame, buffers);
                !result)
            {
                return result;
            }
        }

        const auto vertex_record =
            state.gpu_buffers.find(cache_key(ResourceCacheKey::GLOBAL_VERTICES));
        const auto index_record =
            state.gpu_buffers.find(cache_key(ResourceCacheKey::GLOBAL_INDICES));
        const auto mesh_record = state.gpu_buffers.find(cache_key(ResourceCacheKey::GLOBAL_MESHES));
        buffers.vertices = vertex_record != state.gpu_buffers.end() ? vertex_record->second.resource
                                                                    : INVALID_GPU_ID;
        buffers.index_buffer = index_record != state.gpu_buffers.end()
                                   ? index_record->second.resource
                                   : INVALID_GPU_ID;
        buffers.meshes =
            mesh_record != state.gpu_buffers.end() ? mesh_record->second.resource : INVALID_GPU_ID;

        if (auto result = upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::ALL_INSTANCES),
                "all_instances",
                GraphicsBufferUsage::STORAGE,
                instances,
                buffers.instances);
            !result)
        {
            return result;
        }

        if (auto result = upload_cached_material_buffer(backend, state, buffers); !result)
        {
            return result;
        }

        if (auto result = upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::GLOBAL_LIGHTS),
                "global_lights",
                GraphicsBufferUsage::STORAGE,
                lights,
                buffers.lights);
            !result)
        {
            return result;
        }

        const uint32 cluster_count = CLUSTER_COUNT_X * CLUSTER_COUNT_Y * CLUSTER_COUNT_Z;
        const auto cluster_grid_size =
            static_cast<uint64>(cluster_count) * static_cast<uint64>(sizeof(ShaderClusterGridData));
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::CLUSTER_GRID),
                "cluster_grid",
                GraphicsBufferUsage::STORAGE,
                cluster_grid_size,
                nullptr,
                0U,
                buffers.cluster_grid);
            !result)
        {
            return result;
        }
        const auto light_index_pool_size = static_cast<uint64>(cluster_count)
                                           * static_cast<uint64>(MAX_LIGHTS_PER_CLUSTER)
                                           * static_cast<uint64>(sizeof(uint32));
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::LIGHT_INDEX_POOL),
                "light_index_pool",
                GraphicsBufferUsage::STORAGE,
                light_index_pool_size,
                nullptr,
                0U,
                buffers.light_index_pool);
            !result)
        {
            return result;
        }

        const auto empty_command = ShaderDrawIndexedIndirectCommand {};
        const auto draw_usage = GraphicsBufferUsage::STORAGE | GraphicsBufferUsage::INDIRECT_ARGS;
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::MAIN_DRAW_ARGS),
                "main_draw_args",
                draw_usage,
                byte_size(resources.max_draw_count, sizeof(ShaderDrawIndexedIndirectCommand)),
                &empty_command,
                sizeof(empty_command),
                buffers.main_draw_args);
            !result)
        {
            return result;
        }
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::SHADOW_ARGS_POOL),
                "shadow_args_pool",
                draw_usage,
                byte_size(
                    std::max<size>(static_cast<size>(resources.max_shadow_draw_count), 1U),
                    sizeof(ShaderDrawIndexedIndirectCommand)),
                &empty_command,
                sizeof(empty_command),
                buffers.shadow_draw_args);
            !result)
        {
            return result;
        }

        const auto zero_count = ShaderDrawCount {};
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::MAIN_DRAW_COUNT),
                "main_draw_count",
                draw_usage,
                sizeof(ShaderDrawCount),
                &zero_count,
                sizeof(zero_count),
                buffers.main_draw_count);
            !result)
        {
            return result;
        }
        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::SHADOW_DRAW_COUNT),
                "shadow_draw_count",
                draw_usage,
                sizeof(ShaderDrawCount),
                &zero_count,
                sizeof(zero_count),
                buffers.shadow_draw_count);
            !result)
        {
            return result;
        }

        if (auto result = create_or_update_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::FRAME_UNIFORMS),
                "frame_uniforms",
                GraphicsBufferUsage::UNIFORM,
                sizeof(ShaderSceneUniforms),
                &frame_uniforms,
                sizeof(frame_uniforms),
                buffers.frame_uniforms);
            !result)
        {
            return result;
        }

        const auto gbuffer_texture = make_runtime_texture(
            resources.output_size,
            TextureFormat::RGBA16_FLOAT,
            TextureUsage::SAMPLED_RENDER_TARGET);

        const auto create_gbuffer_target = [&](const ResourceCacheKey key,
                                               const std::string_view debug_name,
                                               GpuId& resource) -> Result
        {
            bool needs_upload = false;
            return create_or_update_texture(
                backend,
                state,
                cache_key(key),
                gbuffer_texture,
                debug_name,
                1U,
                false,
                resource,
                needs_upload);
        };

        if (auto result = create_gbuffer_target(
                ResourceCacheKey::GBUFFER_ALBEDO,
                "gbuffer_albedo",
                buffers.gbuffer_albedo);
            !result)
            return result;
        if (auto result = create_gbuffer_target(
                ResourceCacheKey::GBUFFER_ROUGHNESS,
                "gbuffer_roughness",
                buffers.gbuffer_roughness);
            !result)
            return result;
        if (auto result = create_gbuffer_target(
                ResourceCacheKey::GBUFFER_NORMAL,
                "gbuffer_normal",
                buffers.gbuffer_normal);
            !result)
            return result;
        if (auto result = create_gbuffer_target(
                ResourceCacheKey::GBUFFER_METALLIC,
                "gbuffer_metallic",
                buffers.gbuffer_metallic);
            !result)
            return result;

        const auto depth_texture = make_runtime_texture(
            resources.output_size,
            TextureFormat::DEPTH32_FLOAT,
            TextureUsage::SAMPLED_DEPTH_STENCIL);
        bool needs_upload = false;
        if (auto result = create_or_update_texture(
                backend,
                state,
                cache_key(ResourceCacheKey::FRAME_DEPTH),
                depth_texture,
                "frame_depth",
                1U,
                false,
                buffers.frame_depth,
                needs_upload);
            !result)
        {
            return result;
        }

        const auto final_texture = make_runtime_texture(
            resources.output_size,
            TextureFormat::RGBA16_FLOAT,
            TextureUsage::SAMPLED | TextureUsage::STORAGE);
        needs_upload = false;
        if (auto result = create_or_update_texture(
                backend,
                state,
                cache_key(ResourceCacheKey::FINAL_HDR),
                final_texture,
                "final_hdr",
                1U,
                false,
                buffers.final_hdr,
                needs_upload);
            !result)
        {
            return result;
        }

        const auto shadow_texture = make_runtime_texture(
            Size {
                .width = resources.shadow_map_resolution,
                .height = resources.shadow_map_resolution,
            },
            TextureFormat::DEPTH32_FLOAT,
            TextureUsage::SAMPLED_DEPTH_STENCIL);
        return create_or_update_texture(
            backend,
            state,
            cache_key(ResourceCacheKey::SHADOW_DEPTH_ATLAS),
            shadow_texture,
            "shadow_depth_atlas",
            std::max<uint32>(resources.shadow_count, 1U),
            true,
            buffers.shadow_atlas,
            needs_upload);
    }

    //// INTERNAL SCENE COLLECTION ////

    static Vec3 get_forward(const Transform& transform)
    {
        return glm::normalize(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
    }

    static Vec4 make_light_color(const Light& light)
    {
        return Vec4(light.color.r, light.color.g, light.color.b, light.intensity);
    }

    static ShaderVertexData make_shader_vertex(const Mesh& mesh, const uint32 vertex_index)
    {
        const VertexBuffer& buffer = mesh.vertices;
        const Vec4 position = read_vertex_buffer_attribute(
            buffer,
            vertex_index,
            vertex_attribute_position_debug_name,
            Vec4(0.0F));
        const Vec4 normal = read_vertex_buffer_attribute(
            buffer,
            vertex_index,
            vertex_attribute_normal_debug_name,
            Vec4(0.0F, 1.0F, 0.0F, 0.0F));
        const Vec4 uv = read_vertex_buffer_attribute(
            buffer,
            vertex_index,
            vertex_attribute_uv_debug_name,
            Vec4(0.0F));
        const Vec4 tangent = read_vertex_buffer_attribute(
            buffer,
            vertex_index,
            vertex_attribute_tangent_debug_name,
            Vec4(1.0F, 0.0F, 0.0F, 1.0F));
        const Vec4 color = read_vertex_buffer_attribute(
            buffer,
            vertex_index,
            vertex_attribute_color_debug_name,
            Vec4(1.0F));

        return ShaderVertexData {
            .position = Vec4(position.x, position.y, position.z, 1.0F),
            .normal = Vec4(normal.x, normal.y, normal.z, 0.0F),
            .tangent = tangent,
            .uv = Vec4(uv.x, uv.y, 0.0F, 0.0F),
            .color = color,
        };
    }

    static Vec4 make_plane(const Vec3& normal, const float distance)
    {
        const float length = glm::length(normal);
        if (length <= 0.0F)
            return Vec4(0.0F);

        return Vec4(normal / length, distance / length);
    }

    static std::array<Vec4, 6U> extract_frustum_planes(const Mat4& view_projection)
    {
        const Mat4& m = view_projection;
        return {
            make_plane(
                Vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]),
                m[3][3] + m[3][0]),
            make_plane(
                Vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]),
                m[3][3] - m[3][0]),
            make_plane(
                Vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]),
                m[3][3] + m[3][1]),
            make_plane(
                Vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]),
                m[3][3] - m[3][1]),
            make_plane(
                Vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]),
                m[3][3] + m[3][2]),
            make_plane(
                Vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]),
                m[3][3] - m[3][2]),
        };
    }

    static std::pair<Vec3, Vec3> transform_bounds(const MeshBounds& bounds, const Mat4& model)
    {
        const std::array corners = {
            Vec3(bounds.minimum.x, bounds.minimum.y, bounds.minimum.z),
            Vec3(bounds.maximum.x, bounds.minimum.y, bounds.minimum.z),
            Vec3(bounds.minimum.x, bounds.maximum.y, bounds.minimum.z),
            Vec3(bounds.maximum.x, bounds.maximum.y, bounds.minimum.z),
            Vec3(bounds.minimum.x, bounds.minimum.y, bounds.maximum.z),
            Vec3(bounds.maximum.x, bounds.minimum.y, bounds.maximum.z),
            Vec3(bounds.minimum.x, bounds.maximum.y, bounds.maximum.z),
            Vec3(bounds.maximum.x, bounds.maximum.y, bounds.maximum.z),
        };

        auto minimum = Vec3(std::numeric_limits<float>::max());
        auto maximum = Vec3(std::numeric_limits<float>::lowest());
        for (const Vec3& corner : corners)
        {
            const Vec3 transformed = Vec3(model * Vec4(corner, 1.0F));
            minimum = glm::min(minimum, transformed);
            maximum = glm::max(maximum, transformed);
        }

        return {minimum - Vec3(CULL_BOUNDS_PADDING), maximum + Vec3(CULL_BOUNDS_PADDING)};
    }

    static void append_light(
        const Transform& transform,
        const Light& light,
        const uint32 light_type,
        const float range,
        const Vec4& spot_area,
        std::vector<ShaderLightData>& lights,
        const float shadow_softness,
        uint32& shadow_layer_count)
    {
        const Vec3 direction = get_forward(transform);
        float shadow_layer = -1.0F;
        if (light.cast_shadows && lights.size() < MAX_SHADOW_COUNT)
        {
            const auto layer = static_cast<uint32>(lights.size());
            shadow_layer = static_cast<float>(layer);
            shadow_layer_count = std::max(shadow_layer_count, layer + 1U);
        }

        lights.push_back(
            ShaderLightData {
                .position_range = Vec4(transform.position, range),
                .direction_type = Vec4(direction, static_cast<float>(light_type)),
                .color_intensity = make_light_color(light),
                .spot_angles_area = spot_area,
                .shadow_data = Vec4(shadow_layer, shadow_softness, 0.0015F, 0.0F),
            });
    }

    static void collect_lights(
        World& world,
        std::vector<ShaderLightData>& lights,
        const float shadow_softness,
        uint32& shadow_layer_count)
    {
        for (const auto& entity : world.get_with<Transform, DirectionalLight>())
        {
            append_light(
                get_world_space_transform(entity),
                entity.get_component<DirectionalLight>(),
                SHADER_LIGHT_TYPE_DIRECTIONAL,
                0.0F,
                Vec4(0.0F),
                lights,
                shadow_softness,
                shadow_layer_count);
        }

        for (const auto& entity : world.get_with<Transform, PointLight>())
        {
            const auto& light = entity.get_component<PointLight>();
            append_light(
                get_world_space_transform(entity),
                light,
                SHADER_LIGHT_TYPE_POINT,
                light.range,
                Vec4(0.0F),
                lights,
                shadow_softness,
                shadow_layer_count);
        }

        for (const auto& entity : world.get_with<Transform, SpotLight>())
        {
            const auto& light = entity.get_component<SpotLight>();
            append_light(
                get_world_space_transform(entity),
                light,
                SHADER_LIGHT_TYPE_SPOT,
                light.range,
                Vec4(light.inner_angle, light.outer_angle, 0.0F, 0.0F),
                lights,
                shadow_softness,
                shadow_layer_count);
        }

        for (const auto& entity : world.get_with<Transform, AreaLight>())
        {
            const auto& light = entity.get_component<AreaLight>();
            append_light(
                get_world_space_transform(entity),
                light,
                SHADER_LIGHT_TYPE_AREA,
                light.range,
                Vec4(light.area_size, 0.0F, 0.0F),
                lights,
                shadow_softness,
                shadow_layer_count);
        }
    }

    static uint32 get_mesh_vertex_count(const Mesh& mesh)
    {
        return mesh.get_vertex_stride_float_count() == 0U
                   ? 0U
                   : static_cast<uint32>(
                         mesh.vertices.vertices.size()
                         / static_cast<size>(mesh.get_vertex_stride_float_count()));
    }

    static uint64 hash_vertex_buffer_layout(const VertexBufferLayout& layout)
    {
        auto value = hash(layout.stride);
        value = hash(static_cast<uint64>(layout.elements.size()), value);
        for (const auto& element : layout.elements)
        {
            value = hash(element.debug_name, value);
            value = hash(static_cast<uint32>(element.type), value);
            value = hash(element.offset, value);
            value = hash(element.normalized, value);
        }

        return value;
    }

    static uint64 mesh_cache_key(const DynamicMesh& mesh)
    {
        return hash(static_cast<uint64>(reinterpret_cast<uintptr_t>(mesh.get_data().get())));
    }

    static const Mesh* load_static_mesh(AssetManager& assets, const Handle& handle)
    {
        auto model = assets.load<Model>(handle);
        if ((!model || model->meshes.empty()) && handle != FALLBACK_MESH_HANDLE)
            model = assets.load<Model>(FALLBACK_MESH_HANDLE);
        if (!model || model->meshes.empty())
            return nullptr;

        return &model->meshes.front();
    }

    static Result ensure_material_cached(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const Handle& handle,
        const Material& material,
        MaterialInstance& instance,
        GpuId& out_material_id)
    {
        auto& materials = global_material_data(state);
        const auto material_key = hash_material_instance_key(handle, instance);
        auto iterator = state.material_lookup.find(material_key);
        const bool is_new_material = iterator == state.material_lookup.end();
        if (is_new_material)
        {
            out_material_id = static_cast<uint32>(materials.size());
            state.material_lookup[material_key] = out_material_id;
            materials.push_back(ShaderMaterialData {});
        }
        else
        {
            out_material_id = iterator->second;
        }

        const bool needs_refresh = is_new_material || instance.is_dirty();
        if (needs_refresh)
        {
            materials[out_material_id] =
                make_shader_material(backend, state, assets, handle, material, instance);
            if (auto result = upload_material_entry(backend, state, out_material_id); !result)
                return result;
            instance.clear_dirty();
        }

        return Result(true);
    }

    static std::optional<Vec4> try_make_sky_background_from_contract(
        AssetManager& assets,
        auto& state,
        const Material& material,
        const MaterialInstance& instance)
    {
        auto sky_color = Color::WHITE;
        auto brightness = 1.0F;
        auto background = Vec4(0.55F, 0.72F, 0.9F, 1.0F);

        warn_unused_material_bindings(state, instance.material, material, instance);
        for (const auto& parameter_schema : material.parameters.values)
        {
            if (parameter_schema.target == MaterialBindingTarget::NONE)
            {
                continue;
            }

            const auto parameter =
                find_effective_parameter(material, instance, parameter_schema.name);
            if (!parameter.has_value())
            {
                warn_material_contract_once(
                    state,
                    instance.material,
                    parameter_schema.name,
                    9U,
                    std::format(
                        "Rendering pipeline: sky material '{}' is missing declared parameter '{}'.",
                        instance.material.name,
                        parameter_schema.name));
                return std::nullopt;
            }

            if (!matches_material_parameter_schema(parameter_schema.data, parameter->get().data))
            {
                warn_material_contract_once(
                    state,
                    instance.material,
                    parameter_schema.name,
                    10U,
                    std::format(
                        "Rendering pipeline: sky material '{}' parameter '{}' does not match the declared material schema.",
                        instance.material.name,
                        parameter_schema.name));
                return std::nullopt;
            }

            switch (parameter_schema.target)
            {
                case MaterialBindingTarget::SKY_COLOR:
                {
                    const auto* color = std::get_if<Color>(&parameter->get().data);
                    if (color == nullptr)
                        return std::nullopt;
                    sky_color = *color;
                    break;
                }
                case MaterialBindingTarget::SKY_BRIGHTNESS:
                {
                    if (!try_read_scalar_parameter(parameter->get().data, brightness))
                        return std::nullopt;
                    break;
                }
                default:
                    break;
            }
        }

        for (const auto& texture_schema : material.textures.values)
        {
            if (texture_schema.target != MaterialBindingTarget::SKY_TEXTURE)
                continue;

            const auto texture = find_effective_texture(material, instance, texture_schema.name);
            if (!texture.has_value())
            {
                warn_material_contract_once(
                    state,
                    instance.material,
                    texture_schema.name,
                    11U,
                    std::format(
                        "Rendering pipeline: sky material '{}' is missing declared texture '{}'.",
                        instance.material.name,
                        texture_schema.name));
                return std::nullopt;
            }

            if (!texture->get().texture.id.is_valid())
                continue;

            if (auto sky_texture = assets.load<Texture>(texture->get().texture))
                background = approximate_sky_texture_tint(*sky_texture);
        }

        background = Vec4(sky_color.r, sky_color.g, sky_color.b, 1.0F) * brightness;
        background.a = 1.0F;
        return background;
    }

    static void apply_sky_background_color(
        AssetManager& assets,
        auto& state,
        const Sky& sky,
        FrameBuildResources& frame)
    {
        assets.load<Material>(FALLBACK_MATERIAL_HANDLE);
        auto material = assets.load<Material>(sky.material.material);
        if (!material)
            material = assets.load<Material>(FALLBACK_MATERIAL_HANDLE);

        // Keep the placeholder sky path warm so authored sky entities still exercise the same
        // fallback asset route as the rest of the renderer when content is incomplete.
        assets.load<Model>(FALLBACK_MESH_HANDLE);
        if (!material)
            return;

        if (const auto background =
                try_make_sky_background_from_contract(assets, state, *material, sky.material);
            background.has_value())
        {
            frame.sky_color = *background;
            return;
        }

        frame.sky_color = Vec4(1.0F, 0.0F, 1.0F, 1.0F);
    }

    static Result ensure_mesh_cached(
        auto& state,
        AssetManager& assets,
        const Entity& entity,
        FrameBuildResources& frame,
        bool& out_geometry_changed,
        GpuId& out_mesh_id,
        const Mesh*& out_mesh)
    {
        out_mesh = nullptr;
        if (entity.has_component<DynamicMesh>())
        {
            auto& dynamic_mesh = entity.get_component<DynamicMesh>();
            const auto mesh_data = dynamic_mesh.get_data();
            if (!mesh_data)
                return Result(true);

            out_mesh = &dynamic_mesh.get_mesh();
            const uint64 key = mesh_cache_key(dynamic_mesh);
            auto iterator = state.dynamic_mesh_lookup.find(key);
            if (iterator == state.dynamic_mesh_lookup.end())
            {
                auto record = MeshCacheRecord {
                    .resource = static_cast<GpuId>(global_mesh_records(state).size()),
                };
                out_mesh_id = record.resource;
                state.dynamic_mesh_lookup[key] = out_mesh_id;
                global_mesh_records(state).push_back(record);
                out_geometry_changed = true;
                dynamic_mesh.clear_dirty();
            }
            else
            {
                out_mesh_id = iterator->second;
                if (dynamic_mesh.is_dirty())
                {
                    out_geometry_changed = true;
                    dynamic_mesh.clear_dirty();
                }
            }

            frame.dynamic_mesh_sources[out_mesh_id] = mesh_data;

            return Result(true);
        }

        if (!entity.has_component<StaticMesh>())
            return Result(true);

        const auto& static_mesh = entity.get_component<StaticMesh>();
        out_mesh = load_static_mesh(assets, static_mesh.handle);
        if (out_mesh == nullptr)
            return Result(true);

        auto iterator = state.static_mesh_lookup.find(static_mesh.handle);
        if (iterator == state.static_mesh_lookup.end())
        {
            auto record = MeshCacheRecord {
                .resource = static_cast<GpuId>(global_mesh_records(state).size()),
            };
            out_mesh_id = record.resource;
            state.static_mesh_lookup[static_mesh.handle] = out_mesh_id;
            global_mesh_records(state).push_back(record);
            out_geometry_changed = true;
        }
        else
        {
            out_mesh_id = iterator->second;
        }

        frame.static_mesh_sources[out_mesh_id] = static_mesh.handle;

        return Result(true);
    }

    static Result rebuild_geometry_buffers(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const FrameBuildResources& frame,
        FrameBuffers& buffers)
    {
        auto vertices = std::vector<ShaderVertexData>();
        auto indices = std::vector<uint32>();
        auto& mesh_records = global_mesh_records(state);
        auto meshes = std::vector<ShaderMeshData>(mesh_records.size());

        for (auto& record : mesh_records)
        {
            const Mesh* mesh = nullptr;
            if (const auto dynamic_source = frame.dynamic_mesh_sources.find(record.resource);
                dynamic_source != frame.dynamic_mesh_sources.end())
            {
                const auto dynamic_mesh = dynamic_source->second.lock();
                mesh = dynamic_mesh ? &dynamic_mesh->get_mesh() : nullptr;
            }
            else if (
                const auto static_source = frame.static_mesh_sources.find(record.resource);
                static_source != frame.static_mesh_sources.end())
            {
                mesh = load_static_mesh(assets, static_source->second);
            }

            if (mesh == nullptr || mesh->vertices.empty() || mesh->indices.empty())
            {
                meshes[record.resource] = record.data;
                continue;
            }

            const uint32 first_vertex = static_cast<uint32>(vertices.size());
            const uint32 first_index = static_cast<uint32>(indices.size());
            const uint32 vertex_count = get_mesh_vertex_count(*mesh);
            vertices.reserve(vertices.size() + vertex_count);
            for (uint32 vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
                vertices.push_back(make_shader_vertex(*mesh, vertex_index));

            indices.reserve(indices.size() + mesh->indices.size());
            for (const uint32 index : mesh->indices)
                indices.push_back(index + first_vertex);

            record.data = ShaderMeshData {
                .first_index = first_index,
                .index_count = static_cast<uint32>(mesh->indices.size()),
                .base_vertex = 0,
                .vertex_count = vertex_count,
            };
            meshes[record.resource] = record.data;
        }

        if (auto result = upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::GLOBAL_VERTICES),
                "global_vertices",
                GraphicsBufferUsage::STORAGE,
                vertices,
                buffers.vertices);
            !result)
        {
            return result;
        }
        if (auto result = upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::GLOBAL_INDICES),
                "global_indices",
                GraphicsBufferUsage::INDEX,
                indices,
                buffers.index_buffer);
            !result)
        {
            return result;
        }
        if (auto result = upload_vector_buffer(
                backend,
                state,
                cache_key(ResourceCacheKey::GLOBAL_MESHES),
                "global_meshes",
                GraphicsBufferUsage::STORAGE,
                meshes,
                buffers.meshes);
            !result)
        {
            return result;
        }

        return Result(true);
    }

    static void resolve_frame_camera(
        World& world,
        const Size& output_size,
        Viewport& viewport,
        Mat4& view_projection,
        Vec3& camera_position)
    {
        viewport = Viewport();
        viewport.dimensions = output_size;
        view_projection = Mat4(1.0F);
        camera_position = Vec3(0.0F);

        if (const auto camera_entity = world.first_with<Camera, Transform>();
            camera_entity.get_id().is_valid())
        {
            auto& camera = camera_entity.get_component<Camera>();
            const auto transform = get_world_space_transform(camera_entity);
            camera_position = transform.position;
            camera.set_aspect(
                static_cast<float>(output_size.width) / static_cast<float>(output_size.height));
            view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);
            viewport = camera.get_viewport();
            if (viewport.dimensions.width == 0U || viewport.dimensions.height == 0U)
                viewport.dimensions = output_size;
        }
    }

    static Result extract_frame_resources(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        World& world,
        UploadedFrameResources& resources,
        const float elapsed_time,
        FrameBuildResources& frame,
        bool& out_geometry_changed)
    {
        auto shadow_layer_count = uint32(0U);
        collect_lights(world, frame.lights, resources.shadow_softness, shadow_layer_count);
        if (auto result = upload_fallback_textures(backend, state, assets); !result)
        {
            return result;
        }
        resources.texture_bindings = collect_texture_bindings(state);

        auto& pending_instances = state.pending_instances;
        pending_instances.clear();
        for (const auto& entity : world.get_with<Transform, MaterialInstance>())
        {
            auto& instance = entity.get_component<MaterialInstance>();
            auto material_handle = instance.get_handle().id.is_valid() ? instance.get_handle()
                                                                       : FALLBACK_MATERIAL_HANDLE;
            auto material = material_handle.id.is_valid()
                                ? assets.load<Material>(material_handle)
                                : assets.load<Material>(FALLBACK_MATERIAL_HANDLE);
            if (!material)
            {
                material_handle = FALLBACK_MATERIAL_HANDLE;
                material = assets.load<Material>(material_handle);
            }
            if (!material)
                continue;

            if (auto result = validate_renderable_raster_shader(*material); !result)
            {
                return result;
            }

            auto mesh_id = GpuId();
            auto source_mesh = static_cast<const Mesh*>(nullptr);
            if (auto result = ensure_mesh_cached(
                    state,
                    assets,
                    entity,
                    frame,
                    out_geometry_changed,
                    mesh_id,
                    source_mesh);
                !result)
                return result;

            if (source_mesh == nullptr || source_mesh->vertices.empty()
                || source_mesh->indices.empty())
                continue;

            auto material_id = GpuId();
            if (auto result = ensure_material_cached(
                    backend,
                    state,
                    assets,
                    material_handle,
                    *material,
                    instance,
                    material_id);
                !result)
            {
                return result;
            }

            const auto transform = get_world_space_transform(entity);
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto [bounds_min, bounds_max] =
                transform_bounds(source_mesh->bounds, model_matrix);
            const auto resolved_material_config = resolve_material_config(*material, instance);
            const auto& cached_material = global_material_data(state)[material_id];
            pending_instances.push_back(
                PendingRenderableInstance {
                    .instance =
                        ShaderInstanceData {
                            .model_matrix = model_matrix,
                            .bounds_min = Vec4(bounds_min, 0.0F),
                            .bounds_max = Vec4(bounds_max, 0.0F),
                            .mesh_id = static_cast<uint32>(mesh_id),
                            .material_id = static_cast<uint32>(material_id),
                        },
                    .shader = material->shader,
                    .shader_key = hash_shader_program(material->shader),
                    .pipeline_key =
                        hash_renderable_pipeline_key(material->shader, resolved_material_config),
                    .mesh_id = static_cast<uint32>(mesh_id),
                    .material_id = static_cast<uint32>(material_id),
                    .pipeline_flags = cached_material.pipeline_flags,
                    .material_config = resolved_material_config,
                });
        }

        std::sort(
            pending_instances.begin(),
            pending_instances.end(),
            [](const PendingRenderableInstance& lhs, const PendingRenderableInstance& rhs)
            {
                if (lhs.shader_key != rhs.shader_key)
                    return lhs.shader_key < rhs.shader_key;
                if (lhs.pipeline_key != rhs.pipeline_key)
                    return lhs.pipeline_key < rhs.pipeline_key;
                if (lhs.material_id != rhs.material_id)
                    return lhs.material_id < rhs.material_id;
                return lhs.mesh_id < rhs.mesh_id;
            });

        frame.instances.reserve(pending_instances.size());
        resources.raster_batches.clear();
        for (const auto& pending : pending_instances)
        {
            const auto first_instance = static_cast<uint32>(frame.instances.size());
            frame.instances.push_back(pending.instance);

            if ((pending.pipeline_flags & SHADER_PIPELINE_FLAG_OPAQUE) == 0U)
                continue;

            const bool starts_new_batch =
                resources.raster_batches.empty()
                || resources.raster_batches.back().pipeline_key != pending.pipeline_key
                || resources.raster_batches.back().shader_key != pending.shader_key
                || resources.raster_batches.back().material_id != pending.material_id
                || resources.raster_batches.back().mesh_id != pending.mesh_id;
            if (starts_new_batch)
            {
                resources.raster_batches.push_back(
                    RenderableRasterBatch {
                        .shader = pending.shader,
                        .shader_key = pending.shader_key,
                        .pipeline_key = pending.pipeline_key,
                        .mesh_id = pending.mesh_id,
                        .material_id = pending.material_id,
                        .first_instance = first_instance,
                        .instance_count = 1U,
                        .material_config = pending.material_config,
                    });
            }
            else
            {
                resources.raster_batches.back().instance_count += 1U;
            }
        }

        if (const auto sky_entity = world.first_with<Sky, Transform>();
            sky_entity.get_id().is_valid())
            apply_sky_background_color(assets, state, sky_entity.get_component<Sky>(), frame);

        resources.instance_count = static_cast<uint32>(frame.instances.size());
        resources.shadow_count = shadow_layer_count;
        resources.max_draw_count = resources.instance_count;
        resources.max_shadow_draw_count = resources.instance_count * resources.shadow_count;
        if (resources.instance_count == 0U)
            return Result(true);

        const auto frame_uniforms = ShaderSceneUniforms {
            .view_projection = frame.view_projection,
            .inverse_view_projection = glm::inverse(frame.view_projection),
            .frustum_planes = extract_frustum_planes(frame.view_projection),
            .ambient_light = Vec4(0.04F, 0.04F, 0.04F, 1.0F),
            .camera_position_time = Vec4(frame.camera_position, elapsed_time),
            .shadow_settings = Vec4(
                static_cast<float>(resources.shadow_map_resolution),
                static_cast<float>(MAX_SHADOW_COUNT),
                resources.shadow_softness,
                static_cast<float>(MAX_LIGHTS_PER_CLUSTER)),
            .sky_color = frame.sky_color,
            .screen_size = Vec4(
                static_cast<float>(resources.output_size.width),
                static_cast<float>(resources.output_size.height),
                1.0F / static_cast<float>(resources.output_size.width),
                1.0F / static_cast<float>(resources.output_size.height)),
            .cluster_dimensions = Vec4(
                static_cast<float>(CLUSTER_COUNT_X),
                static_cast<float>(CLUSTER_COUNT_Y),
                static_cast<float>(CLUSTER_COUNT_Z),
                static_cast<float>(CLUSTER_COUNT_X * CLUSTER_COUNT_Y * CLUSTER_COUNT_Z)),
            .current_pass_filter = SHADER_PIPELINE_FLAG_OPAQUE,
            .total_instance_count = resources.instance_count,
            .total_mesh_count = static_cast<uint32>(global_mesh_records(state).size()),
            .total_material_count = static_cast<uint32>(global_material_data(state).size()),
            .light_count = static_cast<uint32>(frame.lights.size()),
            .shadow_count = resources.shadow_count,
            .max_scene_draw_count = resources.max_draw_count,
            .max_shadow_draw_count = resources.max_shadow_draw_count,
        };

        return upload_frame_resources(
            backend,
            state,
            assets,
            frame,
            frame.instances,
            frame.lights,
            frame_uniforms,
            out_geometry_changed,
            resources);
    }

    static Result upload_frame(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        World& world,
        const Window& output,
        IWindowManager& windows,
        const GraphicsSettings& settings,
        const float elapsed_time,
        UploadedFrameResources& resources)
    {
        resources = UploadedFrameResources();
        resources.shadow_map_resolution = sanitize_shadow_map_resolution(settings);
        resources.shadow_softness = sanitize_shadow_softness(settings);
        resources.output_size = windows.get_size(output);
        if (resources.output_size.width == 0U || resources.output_size.height == 0U)
            resources.output_size = Size {.width = 1U, .height = 1U};

        auto& frame = state.frame_build;
        reset_frame_build_resources(frame);
        bool geometry_changed = false;
        resolve_frame_camera(
            world,
            resources.output_size,
            resources.viewport,
            frame.view_projection,
            frame.camera_position);
        return extract_frame_resources(
            backend,
            state,
            assets,
            world,
            resources,
            elapsed_time,
            frame,
            geometry_changed);
    }

    //// INTERNAL COMPUTE PASSES ////

    static Result run_visibility_culling(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        const auto& buffers = resources.buffers;
        auto light_culling = INVALID_GPU_ID;
        auto visibility_culling = INVALID_GPU_ID;
        if (auto result = get_compute_pipeline(
                backend,
                state,
                assets,
                hash(std::string_view("light_culling")),
                "light_culling",
                LIGHT_CULLING_SHADER_HANDLE,
                light_culling);
            !result)
        {
            return result;
        }
        if (auto result = get_compute_pipeline(
                backend,
                state,
                assets,
                hash(std::string_view("visibility_culling")),
                "visibility_culling",
                VISIBILITY_CULLING_SHADER_HANDLE,
                visibility_culling);
            !result)
        {
            return result;
        }

        const auto zero_count = ShaderDrawCount {};
        if (auto result =
                backend.write_buffer(buffers.main_draw_count, &zero_count, sizeof(zero_count), 0U);
            !result)
        {
            return warn_render_pipeline_backend_failure("failed to reset main draw count buffer", result);
        }
        if (auto result =
                backend
                    .write_buffer(buffers.shadow_draw_count, &zero_count, sizeof(zero_count), 0U);
            !result)
        {
            return warn_render_pipeline_backend_failure("failed to reset shadow draw count buffer", result);
        }

        return with_compute_pass(
            backend,
            GraphicsComputePassDesc {.debug_name = "Toybox Compute Submit"},
            [&]() -> Result
            {
                if (auto result = bind_group(
                        backend,
                        state,
                        0U,
                        BindGroupDesc {
                            .bindings = make_common_bindings(buffers),
                            .debug_name = "Toybox Compute Submit Bind Group",
                        });
                    !result)
                {
                    return result;
                }

                if (auto result = backend.bind_compute_pipeline(light_culling); !result)
                    return result;
                if (auto result = backend.dispatch_compute(
                        (CLUSTER_COUNT_X + 3U) / 4U,
                        (CLUSTER_COUNT_Y + 3U) / 4U,
                        (CLUSTER_COUNT_Z + 3U) / 4U);
                    !result)
                {
                    return result;
                }

                if (auto result = backend.bind_compute_pipeline(visibility_culling); !result)
                    return result;

                return backend.dispatch_compute((resources.instance_count + 63U) / 64U, 1U, 1U);
            });
    }

    static Result barrier_compute_to_graphics(
        IGraphicsBackend& backend,
        const FrameBuffers& buffers)
    {
        return backend.pipeline_barrier({
            PipelineBarrierDesc {
                .resource_handle = buffers.main_draw_args,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::INDIRECT_ARGUMENT,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.main_draw_count,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::INDIRECT_ARGUMENT,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.shadow_draw_args,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::INDIRECT_ARGUMENT,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.shadow_draw_count,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::INDIRECT_ARGUMENT,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.cluster_grid,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.light_index_pool,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
        });
    }

    //// INTERNAL RASTER PASSES ////

    static Result render_shadow_atlas(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        if (resources.shadow_count == 0U || resources.instance_count == 0U)
            return Result(true);

        const auto& buffers = resources.buffers;
        auto pipeline = INVALID_GPU_ID;
        const auto shadow_shader = ShaderProgram {
            .vertex = SHADOW_VERTEX_SHADER_HANDLE,
            .fragment = SHADOW_FRAGMENT_SHADER_HANDLE,
        };
        if (auto result = get_raster_pipeline(
                backend,
                state,
                assets,
                hash(std::string_view("shadow_atlas")),
                "shadow_atlas",
                shadow_shader,
                make_shadow_pipeline_desc(),
                pipeline);
            !result)
        {
            return result;
        }

        const uint32 draw_count = resources.max_shadow_draw_count;
        return with_render_pass(
            backend,
            RenderPassDesc {
                .depth_stencil_target = buffers.shadow_atlas,
                .depth_stencil_layer = -1,
                .viewport =
                    Viewport {
                        .dimensions =
                            Size {
                                .width = resources.shadow_map_resolution,
                                .height = resources.shadow_map_resolution,
                            },
                    },
                .clear_depth = 1.0F,
                .clear_flags = GraphicsClearFlags::DEPTH,
                .is_color_write_enabled = false,
                .debug_name = "Toybox Shadow Atlas Pass",
            },
            [&]() -> Result
            {
                if (auto result = backend.bind_raster_pipeline(pipeline); !result)
                    return result;

                auto bindings = make_common_bindings(buffers);
                append_binding(bindings, 0U, buffers.index_buffer);
                if (auto result = bind_group(
                        backend,
                        state,
                        0U,
                        BindGroupDesc {
                            .bindings = bindings,
                            .debug_name = "Toybox Shadow Atlas Bind Group",
                        });
                    !result)
                {
                    return result;
                }

                if (draw_count == 0U)
                    return Result(true);

                return backend.draw_indirect_count(
                    buffers.shadow_draw_args,
                    0U,
                    buffers.shadow_draw_count,
                    0U,
                    draw_count,
                    static_cast<uint32>(sizeof(ShaderDrawIndexedIndirectCommand)));
            });
    }

    static Result barrier_shadow_to_gbuffer(IGraphicsBackend& backend, const FrameBuffers& buffers)
    {
        return backend.pipeline_barrier({
            PipelineBarrierDesc {
                .resource_handle = buffers.shadow_atlas,
                .state_before = ResourceState::DEPTH_WRITE,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
        });
    }

    static Result render_gbuffer(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        if (resources.raster_batches.empty())
            return Result(true);

        const auto& buffers = resources.buffers;
        return with_render_pass(
            backend,
            RenderPassDesc {
                .color_targets =
                    {
                        buffers.gbuffer_albedo,
                        buffers.gbuffer_roughness,
                        buffers.gbuffer_normal,
                        buffers.gbuffer_metallic,
                    },
                .depth_stencil_target = buffers.frame_depth,
                .viewport = resources.viewport,
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                .debug_name = "Toybox GBuffer Pass",
            },
            [&]() -> Result
            {
                auto bindings = make_common_bindings(buffers);
                append_binding(bindings, 0U, buffers.index_buffer);
                bindings.insert(
                    bindings.end(),
                    resources.texture_bindings.begin(),
                    resources.texture_bindings.end());

                auto active_shader_key = uint64(0U);
                auto active_pipeline = INVALID_GPU_ID;
                for (const auto& batch : resources.raster_batches)
                {
                    if (!is_valid_gpu_id(active_pipeline) || active_shader_key != batch.shader_key)
                    {
                        if (auto result = get_raster_pipeline(
                                backend,
                                state,
                                assets,
                                batch.pipeline_key,
                                "gbuffer",
                                batch.shader,
                                make_gbuffer_pipeline_desc(batch.material_config),
                                active_pipeline);
                            !result)
                        {
                            return result;
                        }

                        if (auto result = backend.bind_raster_pipeline(active_pipeline); !result)
                            return result;
                        // OpenGL stores the index buffer binding on the currently bound VAO, so
                        // the shared geometry bind group must be replayed after each pipeline bind.
                        if (auto result = bind_group(
                                backend,
                                state,
                                0U,
                                BindGroupDesc {
                                    .bindings = bindings,
                                    .debug_name = "Toybox GBuffer Bind Group",
                                });
                            !result)
                        {
                            return result;
                        }
                        active_shader_key = batch.shader_key;
                    }

                    const auto& mesh = global_mesh_records(state)[batch.mesh_id].data;
                    if (auto result = backend.draw(
                            mesh.index_count,
                            batch.instance_count,
                            mesh.first_index,
                            mesh.base_vertex,
                            batch.first_instance);
                        !result)
                    {
                        return result;
                    }
                }

                return Result(true);
            });
    }

    static Result barrier_gbuffer_to_compute(IGraphicsBackend& backend, const FrameBuffers& buffers)
    {
        return backend.pipeline_barrier({
            PipelineBarrierDesc {
                .resource_handle = buffers.gbuffer_albedo,
                .state_before = ResourceState::RENDER_TARGET,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.gbuffer_roughness,
                .state_before = ResourceState::RENDER_TARGET,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.gbuffer_normal,
                .state_before = ResourceState::RENDER_TARGET,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.gbuffer_metallic,
                .state_before = ResourceState::RENDER_TARGET,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.frame_depth,
                .state_before = ResourceState::DEPTH_WRITE,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
            PipelineBarrierDesc {
                .resource_handle = buffers.final_hdr,
                .state_before = ResourceState::UNDEFINED,
                .state_after = ResourceState::UNORDERED_ACCESS,
            },
        });
    }

    static Result resolve_lighting(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        const auto& buffers = resources.buffers;
        auto pipeline = INVALID_GPU_ID;
        if (auto result = get_compute_pipeline(
                backend,
                state,
                assets,
                hash(std::string_view("lighting_resolve")),
                "lighting_resolve",
                LIGHTING_RESOLVE_SHADER_HANDLE,
                pipeline);
            !result)
        {
            return result;
        }

        auto layout = INVALID_GPU_ID;
        if (auto result =
                get_bind_group_layout(backend, state, make_lighting_resolve_layout_desc(), layout);
            !result)
        {
            return result;
        }

        return with_compute_pass(
            backend,
            GraphicsComputePassDesc {.debug_name = "Toybox Lighting Resolve"},
            [&]() -> Result
            {
                if (auto result = backend.bind_compute_pipeline(pipeline); !result)
                    return result;

                auto bindings = make_common_bindings(buffers);
                append_gbuffer_bindings(bindings, buffers);
                if (auto result = bind_group(
                        backend,
                        state,
                        0U,
                        BindGroupDesc {
                            .layout_handle = layout,
                            .bindings = bindings,
                            .debug_name = "Toybox Lighting Resolve Bind Group",
                        });
                    !result)
                {
                    return result;
                }

                return backend.dispatch_compute(
                    (resources.output_size.width + 15U) / 16U,
                    (resources.output_size.height + 15U) / 16U,
                    1U);
            });
    }

    static Result barrier_final_to_present(IGraphicsBackend& backend, const FrameBuffers& buffers)
    {
        return backend.pipeline_barrier({
            PipelineBarrierDesc {
                .resource_handle = buffers.final_hdr,
                .state_before = ResourceState::UNORDERED_ACCESS,
                .state_after = ResourceState::SHADER_READ_ONLY,
            },
        });
    }

    //// INTERNAL PRESENTATION ////

    static Result present_frame_texture(
        IGraphicsBackend& backend,
        auto& state,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        const auto& buffers = resources.buffers;
        auto pipeline = INVALID_GPU_ID;
        if (auto result = get_raster_pipeline(
                backend,
                state,
                assets,
                hash(std::string_view("present")),
                "present",
                make_present_shader_program(),
                make_present_pipeline_desc(),
                pipeline);
            !result)
        {
            return result;
        }

        auto layout = INVALID_GPU_ID;
        if (auto result = get_bind_group_layout(backend, state, make_present_layout_desc(), layout);
            !result)
        {
            return result;
        }

        return with_render_pass(
            backend,
            RenderPassDesc {
                .viewport = resources.viewport,
                .clear_color = Color::BLACK,
                .clear_flags = GraphicsClearFlags::COLOR,
                .debug_name = "Toybox Presentation Pass",
            },
            [&]() -> Result
            {
                if (auto result = backend.bind_raster_pipeline(pipeline); !result)
                    return result;

                if (auto result = bind_group(
                        backend,
                        state,
                        0U,
                        BindGroupDesc {
                            .layout_handle = layout,
                            .bindings =
                                {
                                    ResourceBinding {
                                        .binding_slot = SHADER_BINDING_FINAL_HDR,
                                        .resource_handle = buffers.final_hdr,
                                    },
                                },
                            .debug_name = "Toybox Present Bind Group",
                        });
                    !result)
                {
                    return result;
                }

                return backend.draw(3U, 1U, 0U, 0, 0U);
            });
    }


    //// RENDER PIPELINE STAGES ////

    Result FrameUploadStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        World& world,
        const Window& output,
        IWindowManager& window_manager,
        const GraphicsSettings& settings,
        const float elapsed_time,
        UploadedFrameResources& resources)
    {
        return upload_frame(
            backend,
            context,
            assets,
            world,
            output,
            window_manager,
            settings,
            elapsed_time,
            resources);
    }

    Result VisibilityCullingStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        return run_visibility_culling(backend, context, assets, resources);
    }

    Result GpuBarrierStage::execute(
        IGraphicsBackend& backend,
        const FrameBuffers& buffers,
        const GpuBarrierPoint point)
    {
        switch (point)
        {
            case GpuBarrierPoint::VISIBILITY_TO_RASTER:
                return barrier_compute_to_graphics(backend, buffers);
            case GpuBarrierPoint::SHADOW_TO_GBUFFER:
                return barrier_shadow_to_gbuffer(backend, buffers);
            case GpuBarrierPoint::GBUFFER_TO_LIGHTING:
                return barrier_gbuffer_to_compute(backend, buffers);
            case GpuBarrierPoint::LIGHTING_TO_PRESENT:
                return barrier_final_to_present(backend, buffers);
        }

        return make_render_pipeline_failure("Rendering pipeline: unknown GPU barrier point.");
    }

    Result ShadowAtlasStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        return render_shadow_atlas(backend, context, assets, resources);
    }

    Result GBufferStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        return render_gbuffer(backend, context, assets, resources);
    }

    Result LightingResolveStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        return resolve_lighting(backend, context, assets, resources);
    }

    Result PresentStage::execute(
        IGraphicsBackend& backend,
        RenderPipelineContext& context,
        AssetManager& assets,
        const UploadedFrameResources& resources)
    {
        return present_frame_texture(backend, context, assets, resources);
    }

    Result PresentStage::execute_failure_frame(IGraphicsBackend& backend)
    {
        return present_failure_frame(backend);
    }
}
