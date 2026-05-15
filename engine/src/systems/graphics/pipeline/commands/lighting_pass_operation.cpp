#include "tbx/systems/graphics/pipeline/commands/lighting_pass_operation.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "command_building_operation_helpers.h"
#include "pass_operation_helpers.h"
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/frustum.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/shader.h"
#include "tbx/types/trig.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace tbx
{
    LightingPassOperation::LightingPassOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo LightingPassOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Lighting Pass Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    uint32 LightingPassOperation::get_albedo_texture_slot()
    {
        return 0U;
    }

    uint32 LightingPassOperation::get_normal_texture_slot()
    {
        return 1U;
    }

    uint32 LightingPassOperation::get_emissive_texture_slot()
    {
        return 2U;
    }

    uint32 LightingPassOperation::get_material_texture_slot()
    {
        return 3U;
    }

    uint32 LightingPassOperation::get_depth_texture_slot()
    {
        return 4U;
    }

    uint32 LightingPassOperation::get_directional_shadow_texture_slot()
    {
        return 5U;
    }

    uint32 LightingPassOperation::get_point_shadow_texture_slot()
    {
        return 6U;
    }

    uint32 LightingPassOperation::get_spot_shadow_texture_slot()
    {
        return 7U;
    }

    uint32 LightingPassOperation::get_area_shadow_texture_slot()
    {
        return 8U;
    }

    uint32 LightingPassOperation::get_material_uniform_slot()
    {
        return 2U;
    }

    uint32 LightingPassOperation::get_lighting_info_uniform_slot()
    {
        return 1U;
    }

    uint32 LightingPassOperation::get_point_lights_storage_slot()
    {
        return 0U;
    }

    uint32 LightingPassOperation::get_spot_lights_storage_slot()
    {
        return 1U;
    }

    uint32 LightingPassOperation::get_tile_light_spans_storage_slot()
    {
        return 2U;
    }

    uint32 LightingPassOperation::get_tile_point_light_indices_storage_slot()
    {
        return 3U;
    }

    uint32 LightingPassOperation::get_tile_spot_light_indices_storage_slot()
    {
        return 4U;
    }

    uint32 LightingPassOperation::get_area_lights_storage_slot()
    {
        return 5U;
    }

    uint32 LightingPassOperation::get_tile_area_light_indices_storage_slot()
    {
        return 6U;
    }

    uint32 LightingPassOperation::get_directional_shadow_cascades_storage_slot()
    {
        return 7U;
    }

    uint32 LightingPassOperation::get_spot_shadow_maps_storage_slot()
    {
        return 8U;
    }

    uint32 LightingPassOperation::get_area_shadow_maps_storage_slot()
    {
        return 9U;
    }

    Vec4 LightingPassOperation::make_light_radiance(const Light& light)
    {
        const Vec3 color = Vec3(light.color.r, light.color.g, light.color.b);
        return Vec4(color * light.intensity, light.color.a);
    }

    void LightingPassOperation::set_texture_binding(
        std::vector<GraphicsResourceBinding>& bindings,
        const uint32 slot,
        const Uuid resource)
    {
        auto iterator = std::find_if(
            bindings.begin(),
            bindings.end(),
            [slot](const GraphicsResourceBinding& binding)
            {
                return binding.slot == slot;
            });

        if (!resource.is_valid())
        {
            if (iterator != bindings.end())
                bindings.erase(iterator);
            return;
        }

        if (iterator != bindings.end())
        {
            iterator->resource = resource;
            return;
        }

        bindings.push_back(
            GraphicsResourceBinding {
                .slot = slot,
                .resource = resource,
            });
    }

    Result LightingPassOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (data_size == 0U || !data)
            return Result(false, "LightingPassOperation: invalid material uniform data.");

        const auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            _resource_manager.get().update(out_buffer);
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Lighting Material Uniforms",
                },
                data,
                data_size,
                buffer);
            !result)
        {
            return result;
        }

        _material_uniform_buffers[material_key] = buffer;
        out_buffer = buffer;
        return {};
    }

    struct DirectionalLightGpu
    {
        Vec4 direction_ambient = Vec4(0.0F);
        Vec4 radiance_shadowed = Vec4(0.0F);
        IVec4 shadow_info = IVec4(0);
    };

    struct PointLightGpu
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 radiance_shadow_bias = Vec4(0.0F);
        IVec4 shadow_info = IVec4(-1);
    };

    struct SpotLightGpu
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 direction_inner_cos = Vec4(0.0F);
        Vec4 radiance_outer_cos = Vec4(0.0F);
        IVec4 shadow_info = IVec4(-1);
    };

    struct AreaLightGpu
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 direction_half_width = Vec4(0.0F);
        Vec4 radiance_half_height = Vec4(0.0F);
        Vec4 right = Vec4(0.0F);
        Vec4 up = Vec4(0.0F);
        IVec4 shadow_info = IVec4(-1);
    };

    struct TileLightSpanGpu
    {
        UVec4 point_and_spot = UVec4(0U);
        UVec4 area = UVec4(0U);
    };

    struct ShadowCascadeGpu
    {
        Mat4 light_view_projection = Mat4(1.0F);
        Vec4 split_and_bias = Vec4(0.0F);
        IVec4 texture_layer = IVec4(-1);
    };

    struct ProjectedShadowGpu
    {
        Mat4 light_view_projection = Mat4(1.0F);
        Vec4 planes_and_bias = Vec4(0.0F);
        IVec4 texture_layer = IVec4(-1);
    };

    struct LightingInfoGpu
    {
        Vec4 camera_position = Vec4(0.0F);
        Vec4 clear_color = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        IVec4 tile_info = IVec4(0);
        IVec4 light_counts = IVec4(0);
        Mat4 inverse_view_projection = Mat4(1.0F);
        Mat4 view_matrix = Mat4(1.0F);
        DirectionalLightGpu directional_lights[TBX_MAX_DIRECTIONAL_LIGHTS] = {};
    };

    static Result upload_or_update(
        GraphicsResourceManager& resource_manager,
        const GraphicsBufferUsage usage,
        const std::string& debug_name,
        const void* data,
        const uint64 data_size,
        Uuid& buffer,
        uint64& capacity)
    {
        if (data == nullptr || data_size == 0U)
            return Result(false, "LightingPassOperation: invalid buffer upload data.");

        if (!buffer.is_valid() || data_size > capacity)
        {
            if (buffer.is_valid())
                resource_manager.unload(buffer);

            auto created = Uuid {};
            if (const auto result = resource_manager.upload(
                    GraphicsBufferDesc {
                        .usage = usage,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = debug_name,
                    },
                    data,
                    data_size,
                    created);
                !result)
            {
                return result;
            }

            buffer = created;
            capacity = data_size;
            return {};
        }

        return resource_manager.update(buffer, data, data_size, 0U);
    }

    Result LightingPassOperation::ensure_lighting_buffers(
        IGraphicsBackend& backend,
        const RenderData& render_data)
    {
        const auto find_spot_shadow_index = [&render_data](const Uuid entity_uuid) -> int32
        {
            for (uint32 index = 0U; index < render_data.spot_shadow_maps.size(); ++index)
            {
                if (render_data.spot_shadow_maps[index].entity_uuid == entity_uuid)
                    return static_cast<int32>(index);
            }
            return -1;
        };
        const auto find_area_shadow_index = [&render_data](const Uuid entity_uuid) -> int32
        {
            for (uint32 index = 0U; index < render_data.area_shadow_maps.size(); ++index)
            {
                if (render_data.area_shadow_maps[index].entity_uuid == entity_uuid)
                    return static_cast<int32>(index);
            }
            return -1;
        };

        auto point_lights = std::vector<PointLightGpu> {};
        point_lights.reserve(render_data.point_lights.size());
        for (const auto& light : render_data.point_lights)
        {
            point_lights.push_back(
                PointLightGpu {
                    .position_range = Vec4(light.transform.position, light.light.range),
                    .radiance_shadow_bias = Vec4(Vec3(make_light_radiance(light.light)), 0.00035F),
                    .shadow_info = IVec4(-1, 0, 0, 0),
                });
        }

        auto spot_lights = std::vector<SpotLightGpu> {};
        spot_lights.reserve(render_data.spot_lights.size());
        for (const auto& light : render_data.spot_lights)
        {
            const float inner_cos = angle_to_cosine(light.light.inner_angle);
            const float outer_cos = angle_to_cosine(light.light.outer_angle);
            const Vec4 radiance = make_light_radiance(light.light);
            const int32 shadow_index =
                light.light.cast_shadows ? find_spot_shadow_index(light.entity_uuid) : -1;
            spot_lights.push_back(
                SpotLightGpu {
                    .position_range = Vec4(light.transform.position, light.light.range),
                    .direction_inner_cos = Vec4(make_light_direction(light.transform), inner_cos),
                    .radiance_outer_cos = Vec4(Vec3(radiance), outer_cos),
                    .shadow_info = IVec4(shadow_index, 0, 0, 0),
                });
        }

        auto area_lights = std::vector<AreaLightGpu> {};
        area_lights.reserve(render_data.area_lights.size());
        for (const auto& light : render_data.area_lights)
        {
            const Vec3 direction = make_light_direction(light.transform);
            const Vec3 right = normalize(light.transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
            const Vec3 up = normalize(light.transform.rotation * Vec3(0.0F, 1.0F, 0.0F));
            const Vec4 radiance = make_light_radiance(light.light);
            const int32 shadow_index =
                light.light.cast_shadows ? find_area_shadow_index(light.entity_uuid) : -1;
            area_lights.push_back(
                AreaLightGpu {
                    .position_range = Vec4(light.transform.position, light.light.range),
                    .direction_half_width = Vec4(direction, light.light.area_size.x * 0.5F),
                    .radiance_half_height = Vec4(Vec3(radiance), light.light.area_size.y * 0.5F),
                    .right = Vec4(right, 0.0F),
                    .up = Vec4(up, 0.0F),
                    .shadow_info = IVec4(shadow_index, 0, 0, 0),
                });
        }

        auto lighting_info = LightingInfoGpu {
            .camera_position = Vec4(render_data.camera_position, 1.0F),
            .inverse_view_projection = glm::inverse(render_data.view_projection),
            .view_matrix = render_data.camera.get_view_matrix(
                render_data.camera_transform.position,
                render_data.camera_transform.rotation),
        };

        const uint32 directional_count = std::min(
            static_cast<uint32>(render_data.directional_lights.size()),
            TBX_MAX_DIRECTIONAL_LIGHTS);
        const uint32 point_count = static_cast<uint32>(point_lights.size());
        const uint32 spot_count = static_cast<uint32>(spot_lights.size());
        const uint32 area_count = static_cast<uint32>(area_lights.size());
        lighting_info.light_counts = IVec4(directional_count, point_count, spot_count, area_count);
        for (uint32 index = 0U; index < directional_count; ++index)
        {
            const auto& light = render_data.directional_lights[index];
            const bool has_shadows =
                light.entity_uuid == render_data.directional_shadow_light_entity
                && !render_data.directional_shadow_cascades.empty();
            lighting_info.directional_lights[index] = DirectionalLightGpu {
                .direction_ambient =
                    Vec4(make_light_direction(light.transform), light.light.ambient),
                .radiance_shadowed =
                    Vec4(Vec3(make_light_radiance(light.light)), has_shadows ? 1.0F : 0.0F),
                .shadow_info = IVec4(
                    0,
                    has_shadows ? static_cast<int32>(render_data.directional_shadow_cascades.size())
                                : 0,
                    0,
                    0),
            };
        }

        const uint32 tile_size = 16U;
        const uint32 tile_count_x =
            std::max(1U, (render_data.viewport.dimensions.width + tile_size - 1U) / tile_size);
        const uint32 tile_count_y =
            std::max(1U, (render_data.viewport.dimensions.height + tile_size - 1U) / tile_size);
        lighting_info.tile_info = IVec4(
            static_cast<int32>(tile_size),
            static_cast<int32>(tile_count_x),
            static_cast<int32>(tile_count_y),
            0);

        auto tile_light_spans = std::vector<TileLightSpanGpu>(tile_count_x * tile_count_y);
        const auto point_count_u = static_cast<uint32>(point_lights.size());
        const auto spot_count_u = static_cast<uint32>(spot_lights.size());
        const auto area_count_u = static_cast<uint32>(area_lights.size());
        for (auto& span : tile_light_spans)
        {
            span.point_and_spot = UVec4(0U, point_count_u, 0U, spot_count_u);
            span.area = UVec4(0U, area_count_u, 0U, 0U);
        }

        auto tile_point_light_indices = std::vector<uint32> {};
        tile_point_light_indices.reserve(point_count_u);
        for (uint32 index = 0U; index < point_count_u; ++index)
            tile_point_light_indices.push_back(index);

        auto tile_spot_light_indices = std::vector<uint32> {};
        tile_spot_light_indices.reserve(spot_count_u);
        for (uint32 index = 0U; index < spot_count_u; ++index)
            tile_spot_light_indices.push_back(index);

        auto tile_area_light_indices = std::vector<uint32> {};
        tile_area_light_indices.reserve(area_count_u);
        for (uint32 index = 0U; index < area_count_u; ++index)
            tile_area_light_indices.push_back(index);

        auto directional_shadow_cascades = std::vector<ShadowCascadeGpu> {};
        directional_shadow_cascades.reserve(render_data.directional_shadow_cascades.size());
        for (uint32 cascade_index = 0U;
             cascade_index < render_data.directional_shadow_cascades.size();
             ++cascade_index)
        {
            const auto& cascade = render_data.directional_shadow_cascades[cascade_index];
            directional_shadow_cascades.push_back(
                ShadowCascadeGpu {
                    .light_view_projection = cascade.light_view_projection,
                    .split_and_bias = Vec4(
                        cascade.split_depth,
                        cascade.normal_bias,
                        cascade.depth_bias,
                        cascade.blend_distance),
                    .texture_layer = IVec4(static_cast<int32>(cascade_index), 0, 0, 0),
                });
        }

        auto spot_shadow_maps = std::vector<ProjectedShadowGpu> {};
        spot_shadow_maps.reserve(render_data.spot_shadow_maps.size());
        for (const auto& shadow_map : render_data.spot_shadow_maps)
        {
            spot_shadow_maps.push_back(
                ProjectedShadowGpu {
                    .light_view_projection = shadow_map.light_view_projection,
                    .planes_and_bias = Vec4(
                        shadow_map.z_near,
                        shadow_map.z_far,
                        shadow_map.normal_bias,
                        shadow_map.depth_bias),
                    .texture_layer = IVec4(static_cast<int32>(shadow_map.texture_layer), 0, 0, 0),
                });
        }

        auto area_shadow_maps = std::vector<ProjectedShadowGpu> {};
        area_shadow_maps.reserve(render_data.area_shadow_maps.size());
        for (const auto& shadow_map : render_data.area_shadow_maps)
        {
            area_shadow_maps.push_back(
                ProjectedShadowGpu {
                    .light_view_projection = shadow_map.light_view_projection,
                    .planes_and_bias = Vec4(
                        shadow_map.z_near,
                        shadow_map.z_far,
                        shadow_map.normal_bias,
                        shadow_map.depth_bias),
                    .texture_layer = IVec4(static_cast<int32>(shadow_map.texture_layer), 0, 0, 0),
                });
        }

        auto& resource_manager = _resource_manager.get();
        const auto upload_vector = [&resource_manager](
                                       const GraphicsBufferUsage usage,
                                       const std::string& name,
                                       const auto& values,
                                       Uuid& buffer,
                                       uint64& size_bytes) -> Result
        {
            using TValue = typename std::decay_t<decltype(values)>::value_type;
            auto fallback = TValue {};
            const bool has_values = !values.empty();
            const void* data = has_values ? static_cast<const void*>(values.data())
                                          : static_cast<const void*>(&fallback);
            const uint64 bytes = has_values ? static_cast<uint64>(values.size())
                                                  * static_cast<uint64>(sizeof(TValue))
                                            : static_cast<uint64>(sizeof(TValue));
            return upload_or_update(resource_manager, usage, name, data, bytes, buffer, size_bytes);
        };

        if (const auto result = upload_or_update(
                resource_manager,
                GraphicsBufferUsage::UNIFORM,
                "Toybox Lighting Info",
                &lighting_info,
                static_cast<uint64>(sizeof(LightingInfoGpu)),
                _lighting_info_uniform_buffer,
                _lighting_info_uniform_buffer_size);
            !result)
        {
            return result;
        }

        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Point Lights",
                point_lights,
                _point_lights_storage_buffer,
                _point_lights_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Spot Lights",
                spot_lights,
                _spot_lights_storage_buffer,
                _spot_lights_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Tile Light Spans",
                tile_light_spans,
                _tile_light_spans_storage_buffer,
                _tile_light_spans_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Tile Point Light Indices",
                tile_point_light_indices,
                _tile_point_light_indices_storage_buffer,
                _tile_point_light_indices_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Tile Spot Light Indices",
                tile_spot_light_indices,
                _tile_spot_light_indices_storage_buffer,
                _tile_spot_light_indices_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Area Lights",
                area_lights,
                _area_lights_storage_buffer,
                _area_lights_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Tile Area Light Indices",
                tile_area_light_indices,
                _tile_area_light_indices_storage_buffer,
                _tile_area_light_indices_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Directional Shadow Cascades",
                directional_shadow_cascades,
                _directional_shadow_cascades_storage_buffer,
                _directional_shadow_cascades_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Spot Shadow Maps",
                spot_shadow_maps,
                _spot_shadow_maps_storage_buffer,
                _spot_shadow_maps_storage_buffer_size);
            !result)
        {
            return result;
        }
        if (const auto result = upload_vector(
                GraphicsBufferUsage::STORAGE,
                "Toybox Area Shadow Maps",
                area_shadow_maps,
                _area_shadow_maps_storage_buffer,
                _area_shadow_maps_storage_buffer_size);
            !result)
        {
            return result;
        }

        return {};
    }
    Result LightingPassOperation::ensure_render_targets(
        IGraphicsBackend& backend,
        const Size& resolution)
    {
        if (resolution.width == 0U || resolution.height == 0U)
            return Result(false, "LightingPassOperation: render resolution is invalid.");

        const bool matches_resolution = _target_resolution.width == resolution.width
                                        && _target_resolution.height == resolution.height;
        const bool has_all_color_targets = std::all_of(
            _gbuffer_color_targets.begin(),
            _gbuffer_color_targets.end(),
            [](const Uuid target)
            {
                return target.is_valid();
            });
        if (matches_resolution && has_all_color_targets && _gbuffer_depth_target.is_valid())
        {
            for (const Uuid color_target : _gbuffer_color_targets)
                _resource_manager.get().update(color_target);
            _resource_manager.get().update(_gbuffer_depth_target);
            return {};
        }

        for (auto& color_target : _gbuffer_color_targets)
        {
            if (color_target.is_valid())
                _resource_manager.get().unload(color_target);
            color_target = {};
        }
        if (_gbuffer_depth_target.is_valid())
            _resource_manager.get().unload(_gbuffer_depth_target);

        _gbuffer_depth_target = {};
        _target_resolution = {};

        const auto color_target_names = std::array<std::string, 7U> {
            "Toybox GBuffer Color Target",
            "Toybox GBuffer Geometry Preview Target",
            "Toybox GBuffer Albedo Target",
            "Toybox GBuffer Normal Target",
            "Toybox GBuffer Depth Preview Target",
            "Toybox GBuffer Emissive Target",
            "Toybox GBuffer Material Target",
        };
        for (uint32 index = 0U; index < color_target_names.size(); ++index)
        {
            auto& color_target = _gbuffer_color_targets[index];
            if (const auto result = _resource_manager.get().upload(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
                        .format = GraphicsTextureFormat::RGBA16_FLOAT,
                        .size = resolution,
                        .mip_count = 1U,
                        .array_layer_count = 1U,
                        .debug_name = color_target_names[index],
                    },
                    nullptr,
                    0U,
                    color_target);
                !result)
            {
                return result;
            }
        }

        if (const auto result = _resource_manager.get().upload(
                GraphicsTextureDesc {
                    .usage = GraphicsTextureUsage::DEPTH_STENCIL,
                    .format = GraphicsTextureFormat::DEPTH24_STENCIL8,
                    .size = resolution,
                    .mip_count = 1U,
                    .array_layer_count = 1U,
                    .debug_name = "Toybox GBuffer Depth Target",
                },
                nullptr,
                0U,
                _gbuffer_depth_target);
            !result)
        {
            for (auto& color_target : _gbuffer_color_targets)
            {
                if (color_target.is_valid())
                    _resource_manager.get().unload(color_target);
                color_target = {};
            }
            return result;
        }

        _target_resolution = resolution;
        return {};
    }

    Result LightingPassOperation::prepare(RenderData& render_data)
    {
        render_data.gbuffer.clear();
        render_data.lighting_passes.clear();

        const bool has_scene_commands = !render_data.skybox_commands.empty()
                                        || !render_data.opaque_commands.empty()
                                        || !render_data.alpha_cutout_commands.empty()
                                        || !render_data.transparent_commands.empty();
        if (!has_scene_commands)
            return {};

        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "LightingPassOperation requires IGraphicsBackend service.");
        }
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();

        if (const auto result = ensure_render_targets(backend, render_data.render_resolution);
            !result)
            return result;

        if (const auto result = ensure_lighting_buffers(backend, render_data); !result)
            return result;

        auto lighting_material = MaterialInstance(DeferredLightingMaterial::HANDLE);
        auto lighting_resource = GraphicsMaterialDrawResource {};
        if (const auto result = resource_manager.upload(
                lighting_material,
                lighting_resource,
                GraphicsMaterialUploadMode::POST_PROCESS);
            !result)
        {
            return result;
        }

        auto lighting_material_uniform_buffer = Uuid {};
        if (const auto result = ensure_material_uniform_buffer(
                backend,
                lighting_resource.uniform_key,
                lighting_resource.uniform_data.data(),
                lighting_resource.uniform_data.byte_size(),
                lighting_material_uniform_buffer);
            !result)
        {
            return result;
        }

        render_data.gbuffer.color_targets = _gbuffer_color_targets;
        render_data.gbuffer.depth_target = _gbuffer_depth_target;

        auto lighting_textures = std::move(lighting_resource.textures);
        set_texture_binding(
            lighting_textures,
            get_albedo_texture_slot(),
            render_data.gbuffer.get_albedo_target());
        set_texture_binding(
            lighting_textures,
            get_normal_texture_slot(),
            render_data.gbuffer.get_normal_target());
        set_texture_binding(
            lighting_textures,
            get_emissive_texture_slot(),
            render_data.gbuffer.get_emissive_target());
        set_texture_binding(
            lighting_textures,
            get_material_texture_slot(),
            render_data.gbuffer.get_material_target());
        set_texture_binding(
            lighting_textures,
            get_depth_texture_slot(),
            render_data.gbuffer.depth_target);
        set_texture_binding(
            lighting_textures,
            get_directional_shadow_texture_slot(),
            render_data.directional_shadow_texture);
        set_texture_binding(
            lighting_textures,
            get_point_shadow_texture_slot(),
            render_data.point_shadow_texture);
        set_texture_binding(
            lighting_textures,
            get_spot_shadow_texture_slot(),
            render_data.spot_shadow_texture);
        set_texture_binding(
            lighting_textures,
            get_area_shadow_texture_slot(),
            render_data.area_shadow_texture);

        const bool has_enabled_post_process = [&render_data]()
        {
            if (!render_data.post_processing.is_enabled)
                return false;

            for (const auto& effect : render_data.post_processing.effects)
            {
                if (effect.is_enabled && effect.material.get_handle().is_valid())
                    return true;
            }

            return false;
        }();

        auto lighting_pass_desc = GraphicsPassDesc {
            .clear_color = Color::BLACK,
            .clear_depth = 1.0F,
            .clear_flags = GraphicsClearFlags::COLOR,
            .debug_name = "Toybox Lighting Pass",
        };
        if (has_enabled_post_process)
            lighting_pass_desc.color_targets = {render_data.gbuffer.get_color_target()};

        if (!render_data.fullscreen_quad_vertex_buffer.is_valid()
            || !render_data.fullscreen_quad_index_buffer.is_valid()
            || render_data.fullscreen_quad_index_count == 0U)
        {
            return Result(
                false,
                "LightingPassOperation: fullscreen quad resources are unavailable.");
        }

        render_data.lighting_passes.push_back(
            GraphicsRenderPass {
                .pass = std::move(lighting_pass_desc),
                .viewport = render_data.viewport,
                .indexed_draws =
                    {
                        GraphicsIndexedDrawCommand {
                            .pipeline = lighting_resource.pipeline,
                            .vertex_buffers =
                                {
                                    GraphicsResourceBinding {
                                        .slot = 0U,
                                        .resource = render_data.fullscreen_quad_vertex_buffer},
                                },
                            .index_buffer = render_data.fullscreen_quad_index_buffer,
                            .index_type = GraphicsIndexType::UINT32,
                            .uniform_buffers =
                                {
                                    GraphicsResourceBinding {
                                        .slot = get_material_uniform_slot(),
                                        .resource = lighting_material_uniform_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_lighting_info_uniform_slot(),
                                        .resource = _lighting_info_uniform_buffer},
                                },
                            .storage_buffers =
                                {
                                    GraphicsResourceBinding {
                                        .slot = get_point_lights_storage_slot(),
                                        .resource = _point_lights_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_spot_lights_storage_slot(),
                                        .resource = _spot_lights_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_tile_light_spans_storage_slot(),
                                        .resource = _tile_light_spans_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_tile_point_light_indices_storage_slot(),
                                        .resource = _tile_point_light_indices_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_tile_spot_light_indices_storage_slot(),
                                        .resource = _tile_spot_light_indices_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_area_lights_storage_slot(),
                                        .resource = _area_lights_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_tile_area_light_indices_storage_slot(),
                                        .resource = _tile_area_light_indices_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_directional_shadow_cascades_storage_slot(),
                                        .resource = _directional_shadow_cascades_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_spot_shadow_maps_storage_slot(),
                                        .resource = _spot_shadow_maps_storage_buffer},
                                    GraphicsResourceBinding {
                                        .slot = get_area_shadow_maps_storage_slot(),
                                        .resource = _area_shadow_maps_storage_buffer},
                                },
                            .textures = std::move(lighting_textures),
                            .draw =
                                GraphicsDrawIndexedDesc {
                                    .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                    .index_type = GraphicsIndexType::UINT32,
                                    .index_count = render_data.fullscreen_quad_index_count,
                                    .instance_count = 1U,
                                },
                        },
                    },
            });

        return {};
    }

    Result LightingPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_render_pass_list(
            backend,
            render_data,
            render_data.lighting_passes,
            token,
            "LightingPassOperation");
    }
}
