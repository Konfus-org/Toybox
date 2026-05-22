#pragma once
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/graphics/render_pass.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/post_processing.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/render_target.h"
#include "tbx/types/trig.h"
#include "tbx/types/viewport.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tbx::internal
{
    constexpr float SHADOW_DEPTH_BIAS = 0.0015F;
    constexpr float SHADOW_NORMAL_BIAS = 0.035F;
    constexpr float SHADOW_STRENGTH = 0.75F;
    constexpr float SHADOW_SLOPE_BIAS = 0.0025F;
    constexpr float SHADOW_NEAR_PLANE = 0.1F;
    constexpr float SHADOW_DEPTH_PADDING = 16.0F;

    struct GBuffer
    {
        GraphicsResourceBinding albedo = {};
        GraphicsResourceBinding normal = {};
        GraphicsResourceBinding material = {};
        GraphicsResourceBinding emissive = {};
        GraphicsResourceBinding depth = {};
        GraphicsResourceBinding final_color = {};
    };

    struct RenderCamera
    {
        Mat4 view = Mat4(1.0F);
        Vec3 position = Vec3(0.0F);
        Mat4 projection = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view = Mat4(1.0F);
        Mat4 inverse_projection = Mat4(1.0F);
    };

    struct RenderLight
    {
        uint type = 0U;
        Color color = Color::WHITE;
        float intensity = 0.0F;
        float ambient = 0.0F;
        float inner_cone = 0.0F;
        float outer_cone = 0.0F;
        int32 shadow_index = -1;
        uint32 shadow_layer_count = 0U;
        Vec3 position = Vec3(0.0F);
        Vec3 direction = Vec3(0.0F);
        float range = 0.0F;
    };

    struct RenderShadows
    {
        uint32 layer_count = 0U;
        GraphicsResourceBinding map = {};
    };

    struct RenderMesh
    {
        using Data = std::variant<StaticMesh, DynamicMesh>;

        Handle handle = {};
        Data data = StaticMesh();
    };

    struct RenderMeshInstance
    {
        Mat4 model_matrix = Mat4(1.0F);
        Mat4 normal_matrix = Mat4(1.0F);
    };

    struct RenderBatch
    {
        Uuid pipeline = {};
        RenderMesh mesh = {};
        MaterialInstance material = {};
        std::vector<RenderMeshInstance> instances = {};
    };

    struct FrameData
    {
        uint index = 0U;
        float time = 0.0F;
        float delta_time = 0.0F;
        Size resolution = {};
        Viewport viewport = {};
        RenderTarget target = {};
        RenderCamera camera = {};
        GBuffer g_buffer = {};
    };

    struct RenderLighting
    {
        std::vector<RenderLight> lights = {};
        uint32 shadow_layer_count = 0U;
    };

    struct RenderDrawData
    {
        using BatchMap = std::unordered_map<uint64, RenderBatch>;

        Sky sky = {};

        RenderLighting lighting = {};
        RenderShadows shadows = {};
        PostProcessing post_processing = {};

        BatchMap opaque_batches = {};
        BatchMap transparent_batches = {};
        BatchMap shadow_batches = {};

        Color clear_color = Color::BLACK;
    };

    static int32 reserve_shadow_index(
        uint32& shadow_layer_count,
        const bool casts_shadows,
        const uint32 layer_count)
    {
        if (!casts_shadows || layer_count == 0U || shadow_layer_count + layer_count > MAX_LIGHTS)
            return -1;

        const int32 result = static_cast<int32>(shadow_layer_count);
        shadow_layer_count += layer_count;
        return result;
    }

    static bool has_length(const Vec3& value)
    {
        return dot(value, value) > 0.000001F;
    }

    static Vec3 get_shadow_up_vector(const Vec3& direction)
    {
        // TODO: Lets make a new Struct that inherits from the glm vec types and introduce a
        // Vec3::UP, DOWN, LEFT, RIGHT
        const Vec3 world_up = Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(dot(normalize_or_zero(direction), world_up)) < 0.95F)
            return world_up;

        return Vec3(1.0F, 0.0F, 0.0F);
    }

    static Vec3 project_camera_frustum_corner(
        const RenderCamera& camera,
        const float clip_x,
        const float clip_y,
        const float view_depth)
    {
        Vec4 view_corner = camera.inverse_projection * Vec4(clip_x, clip_y, 1.0F, 1.0F);
        view_corner /= std::abs(view_corner.w) > 0.000001F ? view_corner.w : 1.0F;

        const float scale = view_depth / std::max(-view_corner.z, 0.000001F);
        view_corner = Vec4(Vec3(view_corner) * scale, 1.0F);

        const Vec4 world_corner = camera.inverse_view * view_corner;
        return Vec3(world_corner);
    }

    // TODO: Implement this as a part of the camera class, actually implement a dediecated "Frustum"
    // struct and move this into a 'Frustum.get_corners' do the same with the simgular version
    static std::array<Vec3, 8U> make_camera_frustum_corners(
        const RenderCamera& camera,
        const float split_near,
        const float split_far)
    {
        return std::array<Vec3, 8U> {
            project_camera_frustum_corner(camera, -1.0F, -1.0F, split_near),
            project_camera_frustum_corner(camera, 1.0F, -1.0F, split_near),
            project_camera_frustum_corner(camera, 1.0F, 1.0F, split_near),
            project_camera_frustum_corner(camera, -1.0F, 1.0F, split_near),
            project_camera_frustum_corner(camera, -1.0F, -1.0F, split_far),
            project_camera_frustum_corner(camera, 1.0F, -1.0F, split_far),
            project_camera_frustum_corner(camera, 1.0F, 1.0F, split_far),
            project_camera_frustum_corner(camera, -1.0F, 1.0F, split_far),
        };
    }

    static Mat4 make_directional_shadow_matrix(
        const RenderCamera& camera,
        const Vec3& light_direction,
        const float split_near,
        const float split_far)
    {
        const auto corners = make_camera_frustum_corners(camera, split_near, split_far);
        auto center = Vec3(0.0F);
        for (const Vec3& corner : corners)
            center += corner;
        center *= 1.0F / static_cast<float>(corners.size());

        auto radius = 0.0F;
        for (const Vec3& corner : corners)
            radius = std::max(radius, distance(center, corner));
        radius = std::max(radius, 1.0F);

        const Vec3 direction =
            has_length(light_direction) ? normalize(light_direction) : Vec3(0.0F, -1.0F, 0.0F);
        const Mat4 light_view = look_at(
            center - direction * (radius + SHADOW_DEPTH_PADDING),
            center,
            get_shadow_up_vector(direction));

        auto min_bounds = Vec3(std::numeric_limits<float>::max());
        auto max_bounds = Vec3(std::numeric_limits<float>::lowest());
        for (const Vec3& corner : corners)
        {
            const Vec3 light_space_corner = Vec3(light_view * Vec4(corner, 1.0F));
            min_bounds = glm::min(min_bounds, light_space_corner);
            max_bounds = glm::max(max_bounds, light_space_corner);
        }

        const Mat4 light_projection = ortho_projection(
            min_bounds.x,
            max_bounds.x,
            min_bounds.y,
            max_bounds.y,
            std::max(0.01F, -max_bounds.z - SHADOW_DEPTH_PADDING),
            std::max(0.02F, -min_bounds.z + SHADOW_DEPTH_PADDING));
        return light_projection * light_view;
    }

    static Mat4 make_local_shadow_matrix(const RenderCamera& camera, const RenderLight& light)
    {
        Vec3 direction = light.direction;
        if (!has_length(direction))
            direction = camera.position - light.position;
        if (!has_length(direction))
            direction = Vec3(0.0F, 0.0F, -1.0F);

        direction = normalize(direction);
        const Mat4 light_view =
            look_at(light.position, light.position + direction, get_shadow_up_vector(direction));
        const Mat4 light_projection = perspective_projection(
            to_radians(90.0F),
            1.0F,
            SHADOW_NEAR_PLANE,
            std::max(light.range, 1.0F));
        return light_projection * light_view;
    }

    static ShadowShaderData make_shadow_shader_data(
        const RenderDrawData& draw_data,
        const FrameData& frame,
        const float shadow_render_distance,
        const float shadow_softness)
    {
        auto shadow_data = ShadowShaderData();
        shadow_data.shadow_meta = IVec4(static_cast<int32>(draw_data.shadows.layer_count), 0, 0, 0);

        const float directional_shadow_distance = std::max(shadow_render_distance, 1.0F);
        const float cascade_length =
            directional_shadow_distance / static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT);

        for (const RenderLight& light : draw_data.lighting.lights)
        {
            if (light.shadow_index < 0 || light.shadow_layer_count == 0U)
                continue;

            if (light.type == static_cast<uint>(SHADER_LIGHT_TYPE_DIRECTIONAL))
            {
                float split_near = SHADOW_NEAR_PLANE;
                for (uint32 cascade = 0U; cascade < light.shadow_layer_count; ++cascade)
                {
                    const uint32 layer = static_cast<uint32>(light.shadow_index) + cascade;
                    const float split_far = std::min(
                        directional_shadow_distance,
                        cascade_length * static_cast<float>(cascade + 1U));
                    const float blend_size =
                        std::min(cascade_length * 0.2F, shadow_softness * 4.0F);

                    shadow_data.light_view_projections[layer] = make_directional_shadow_matrix(
                        frame.camera,
                        light.direction,
                        split_near,
                        split_far);
                    shadow_data.light_directions[layer] = Vec4(light.direction, 0.0F);
                    shadow_data.shadow_params[layer] = Vec4(
                        SHADOW_DEPTH_BIAS,
                        SHADOW_NORMAL_BIAS,
                        SHADOW_STRENGTH,
                        SHADOW_SLOPE_BIAS);
                    shadow_data.shadow_extra_params[layer] = Vec4(
                        split_near,
                        split_far,
                        std::max(split_near, split_far - blend_size),
                        static_cast<float>(light.shadow_layer_count));
                    split_near = split_far;
                }
                continue;
            }

            const uint32 layer = static_cast<uint32>(light.shadow_index);
            shadow_data.light_view_projections[layer] =
                make_local_shadow_matrix(frame.camera, light);
            shadow_data.light_directions[layer] = Vec4(light.direction, 0.0F);
            shadow_data.shadow_params[layer] =
                Vec4(SHADOW_DEPTH_BIAS, SHADOW_NORMAL_BIAS, SHADOW_STRENGTH, SHADOW_SLOPE_BIAS);
            shadow_data.shadow_extra_params[layer] = Vec4(
                SHADOW_NEAR_PLANE,
                std::max(light.range, 1.0F),
                std::max(light.range - shadow_softness, SHADOW_NEAR_PLANE),
                static_cast<float>(light.shadow_layer_count));
        }

        return shadow_data;
    }

    static ShadowShaderData make_shadow_shader_data_for_layer(
        const ShadowShaderData& frame_shadow_data,
        const uint32 active_shadow_layer)
    {
        auto layer_shadow_data = frame_shadow_data;
        layer_shadow_data.shadow_meta.y = static_cast<int32>(active_shadow_layer);
        return layer_shadow_data;
    }

    static MaterialConfig resolve_draw_material_config(
        AssetManager& asset_manager,
        const MaterialInstance& material)
    {
        if (!material.get_handle().is_valid())
            return MaterialConfig();

        if (material.has_config_override_enabled())
            return material.overrides.config;

        const auto loaded_material =
            asset_manager.load<Material>(material.get_handle(), MaterialLoadParameters());

        return loaded_material ? loaded_material->config : MaterialConfig();
    }

    static void append_light(RenderDrawData& draw_data, const RenderLight& render_light)
    {
        if (draw_data.lighting.lights.size() < MAX_LIGHTS)
            draw_data.lighting.lights.push_back(render_light);
    }

    static void append_batch(
        RenderDrawData::BatchMap& batches,
        const uint64 key,
        const RenderMesh& mesh,
        const MaterialInstance& material,
        const RenderMeshInstance& instance)
    {
        auto& batch = batches
                          .try_emplace(
                              key,
                              RenderBatch {
                                  .mesh = mesh,
                                  .material = material,
                              })
                          .first->second;
        batch.instances.push_back(instance);
    }

    static uint64 make_batch_hash(
        const Uuid& mesh_id,
        const DynamicMeshData* dynamic_mesh,
        const uint64 material_key)
    {
        uint64 result = hash(mesh_id, TBX_FNV1A_OFFSET_BASIS);
        result = hash(static_cast<uint64>(reinterpret_cast<std::uintptr_t>(dynamic_mesh)), result);
        result = hash(material_key, result);
        return result == 0U ? 1U : result;
    }

    static Transform get_optional_transform(Entity& entity)
    {
        // Components may omit Transform; identity keeps those renderables deterministic.
        if (entity.has_component<Transform>())
            return get_world_space_transform(entity);

        return Transform();
    }

    static bool is_within_local_light_range(
        const Vec3& position,
        const Vec3& camera_position,
        const float max_distance)
    {
        return max_distance <= 0.0F || distance(position, camera_position) <= max_distance;
    }

    static bool should_cast_shadow(
        const MaterialConfig& config,
        const Vec3& position,
        const Vec3& camera_position,
        const float max_distance)
    {
        if (config.shadow_mode == ShadowMode::NONE)
            return false;

        return config.shadow_mode == ShadowMode::ALWAYS || max_distance <= 0.0F
               || distance(position, camera_position) <= max_distance;
    }

    static LightingShaderData make_light_shader_data(const RenderDrawData& draw_data)
    {
        auto light_data = LightingShaderData();
        light_data.light_meta.x = static_cast<int32>(draw_data.lighting.lights.size());
        light_data.light_meta.y = static_cast<int32>(draw_data.shadows.layer_count);

        auto ambient_color_sum = Vec3(0.0F);
        float ambient_intensity_sum = 0.0F;
        uint32 directional_light_count = 0U;
        for (const auto& light : draw_data.lighting.lights)
        {
            if (light.type != static_cast<uint>(SHADER_LIGHT_TYPE_DIRECTIONAL))
                continue;

            ambient_color_sum += Vec3(light.color.r, light.color.g, light.color.b);
            ambient_intensity_sum += light.ambient;
            ++directional_light_count;
        }

        if (directional_light_count > 0U)
        {
            const Vec3 ambient_color =
                ambient_color_sum * (1.0F / static_cast<float>(directional_light_count));
            light_data.ambient_color = Vec4(ambient_color * ambient_intensity_sum, 1.0F);
        }

        for (uint light_index = 0U;
             light_index < static_cast<uint>(draw_data.lighting.lights.size());
             ++light_index)
        {
            const auto& light = draw_data.lighting.lights[static_cast<size>(light_index)];
            light_data.lights[light_index] = ShaderLightData {
                .position_type = Vec4(light.position, static_cast<float>(light.type)),
                .direction_range = Vec4(light.direction, light.range),
                .color_intensity =
                    Vec4(light.color.r, light.color.g, light.color.b, light.intensity),
                .params = Vec4(
                    light.inner_cone,
                    light.outer_cone,
                    static_cast<float>(light.shadow_index),
                    static_cast<float>(light.shadow_layer_count)),
            };
        }

        return light_data;
    }

    static bool has_texture_slot(
        const std::vector<GraphicsResourceBinding>& textures,
        const uint32 slot)
    {
        return std::any_of(
            textures.begin(),
            textures.end(),
            [slot](const GraphicsResourceBinding& texture)
            {
                return texture.slot == slot && texture.resource.is_valid();
            });
    }

    static void append_fallback_texture(
        RenderingResourceManager& resource_manager,
        const uint32 binding_id,
        std::vector<GraphicsResourceBinding>& textures)
    {
        const auto slot = resolve_shader_texture_slot(binding_id);
        if (!slot.has_value() || has_texture_slot(textures, *slot))
            return;

        const auto texture = resource_manager.upload_fallback_texture(binding_id);
        if (texture.resource.is_valid())
            textures.push_back(texture);
    }

    static void append_pbr_fallback_textures(
        RenderingResourceManager& resource_manager,
        RenderingMaterialUploadData& material_upload)
    {
        append_fallback_texture(resource_manager, PARAM_ALBEDO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_NORMAL_MAP, material_upload.textures);
        append_fallback_texture(
            resource_manager,
            PARAM_METALLIC_ROUGHNESS_MAP,
            material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_AO_MAP, material_upload.textures);
        append_fallback_texture(resource_manager, PARAM_EMISSIVE_MAP, material_upload.textures);
    }

    static Result upload_batch_material(
        RenderingResourceManager& resource_manager,
        const MaterialInstance& material,
        RenderingMaterialUploadData& out_material)
    {
        const Result result = material.get_handle().is_valid()
                                  ? resource_manager.upload_material(material, out_material)
                                  : resource_manager.upload_fallback_material(out_material);
        if (result)
            append_pbr_fallback_textures(resource_manager, out_material);

        return result;
    }

    static Result upload_batch_meshes(
        RenderingResourceManager& resource_manager,
        const RenderMesh& mesh,
        std::vector<RenderingMeshUploadData>& out_meshes)
    {
        auto result = Result();
        if (std::holds_alternative<StaticMesh>(mesh.data))
        {
            result = mesh.handle.is_valid() ? resource_manager.upload_model(mesh.handle, out_meshes)
                                            : resource_manager.upload_fallback_mesh(out_meshes);
        }
        else
        {
            const auto& dynamic_mesh = std::get<DynamicMesh>(mesh.data);
            const auto mesh_data = dynamic_mesh.get_data();
            if (mesh_data && !dynamic_mesh.get_mesh().vertices.empty()
                && !dynamic_mesh.get_mesh().indices.empty())
            {
                auto uploaded_mesh = RenderingMeshUploadData();
                result = resource_manager.upload_dynamic_mesh(mesh_data, uploaded_mesh);
                if (result)
                    out_meshes.push_back(uploaded_mesh);
            }
            else
            {
                result = resource_manager.upload_fallback_mesh(out_meshes);
            }
        }

        if (result && out_meshes.empty())
            result = resource_manager.upload_fallback_mesh(out_meshes);

        return result;
    }

    static Result append_batch_draws(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const GraphicsResourceBinding* shadow_uniform,
        const GraphicsResourceBinding* shadow_map,
        const RenderDrawData::BatchMap& batches,
        RenderPass& render_pass)
    {
        for (const auto& [batch_key, batch] : batches)
        {
            if (batch.instances.empty())
                continue;

            auto material_upload = RenderingMaterialUploadData();
            auto result = upload_batch_material(resource_manager, batch.material, material_upload);
            if (!result)
                return result;

            // Object and instance data are uploaded together here so the batch stays CPU-friendly
            // until it is converted into the backend draw contract.
            const auto object_data = ObjectShaderData {
                .model = batch.instances.front().model_matrix,
                .normal_matrix = batch.instances.front().normal_matrix,
            };
            const auto object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                std::string("Toybox/Uniforms/Object/") + std::to_string(batch_key),
                frame_index,
                &object_data,
                static_cast<uint64>(sizeof(object_data)));
            const auto material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                std::string("Toybox/Uniforms/Material/") + std::to_string(hash(batch.material)),
                frame_index,
                material_upload.uniform_values.data(),
                static_cast<uint64>(material_upload.uniform_values.size() * sizeof(Vec4)));
            const auto instance_buffer = resource_manager.upload_instance_buffer(
                std::string("Toybox/Instances/") + std::to_string(batch_key),
                frame_index,
                batch.instances.data(),
                static_cast<uint64>(batch.instances.size() * sizeof(RenderMeshInstance)));
            if (!material_upload.pipeline.is_valid() || !object_uniform.resource.is_valid()
                || !material_uniform.resource.is_valid() || !instance_buffer.resource.is_valid())
            {
                return Result(false, "Rendering pipeline failed: draw upload failed.");
            }

            auto uploaded_meshes = std::vector<RenderingMeshUploadData>();
            result = upload_batch_meshes(resource_manager, batch.mesh, uploaded_meshes);
            if (!result)
                return result;

            for (const auto& uploaded_mesh : uploaded_meshes)
            {
                render_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = material_upload.pipeline,
                        .index_buffer = uploaded_mesh.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = VERTEX_BUFFER_SLOT_MESH,
                                    .resource = uploaded_mesh.vertex_buffer,
                                },
                                instance_buffer,
                            },
                        .uniform_buffers =
                            {
                                frame_uniform,
                                camera_uniform,
                                light_uniform,
                                object_uniform,
                                material_uniform,
                            },
                        .textures = material_upload.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = uploaded_mesh.index_count,
                                .instance_count = static_cast<uint32>(batch.instances.size()),
                            },
                    });
                if (shadow_uniform != nullptr && shadow_uniform->resource.is_valid())
                    render_pass.indexed_draws.back().uniform_buffers.push_back(*shadow_uniform);
                if (shadow_map != nullptr && shadow_map->resource.is_valid())
                    render_pass.indexed_draws.back().textures.push_back(*shadow_map);
            }
        }

        return {};
    }

    static Result append_shadow_draws(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const GraphicsResourceBinding& shadow_uniform,
        const RenderingMaterialUploadData& shadow_material_upload,
        const GraphicsResourceBinding& shadow_material_uniform,
        const RenderDrawData::BatchMap& batches,
        RenderPass& render_pass)
    {
        for (const auto& [batch_key, batch] : batches)
        {
            if (batch.instances.empty())
                continue;

            const auto object_data = ObjectShaderData {
                .model = batch.instances.front().model_matrix,
                .normal_matrix = batch.instances.front().normal_matrix,
            };
            const auto object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                std::string("Toybox/Uniforms/ShadowObject/") + std::to_string(batch_key),
                frame_index,
                &object_data,
                static_cast<uint64>(sizeof(object_data)));
            const auto instance_buffer = resource_manager.upload_instance_buffer(
                std::string("Toybox/ShadowInstances/") + std::to_string(batch_key),
                frame_index,
                batch.instances.data(),
                static_cast<uint64>(batch.instances.size() * sizeof(RenderMeshInstance)));
            if (!object_uniform.resource.is_valid() || !instance_buffer.resource.is_valid())
                return Result(false, "Rendering pipeline failed: shadow draw upload failed.");

            auto uploaded_meshes = std::vector<RenderingMeshUploadData>();
            auto result = upload_batch_meshes(resource_manager, batch.mesh, uploaded_meshes);
            if (!result)
                return result;

            for (const auto& uploaded_mesh : uploaded_meshes)
            {
                render_pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = shadow_material_upload.pipeline,
                        .index_buffer = uploaded_mesh.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = VERTEX_BUFFER_SLOT_MESH,
                                    .resource = uploaded_mesh.vertex_buffer,
                                },
                                instance_buffer,
                            },
                        .uniform_buffers =
                            {
                                frame_uniform,
                                camera_uniform,
                                light_uniform,
                                object_uniform,
                                shadow_material_uniform,
                                shadow_uniform,
                            },
                        .textures = shadow_material_upload.textures,
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = uploaded_mesh.index_count,
                                .instance_count = static_cast<uint32>(batch.instances.size()),
                            },
                    });
            }
        }

        return {};
    }

    static GraphicsTextureDesc make_shadow_map_desc(
        const uint32 resolution,
        const uint32 layer_count)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
            .format = GraphicsTextureFormat::DEPTH32_FLOAT,
            .size =
                Size {
                    .width = std::max(resolution, 1U),
                    .height = std::max(resolution, 1U),
                },
            .mip_count = 1U,
            .array_layer_count = std::max(layer_count, 1U),
            .debug_name = "Toybox Shadow Map",
        };
    }

    static Result create_shadow_map(
        RenderingResourceManager& resource_manager,
        const GraphicsSettings& settings,
        RenderDrawData& draw_data)
    {
        if (draw_data.shadows.layer_count == 0U)
            return {};

        draw_data.shadows.map = resource_manager.upload_texture(
            BINDING_SHADOW_MAP,
            "Toybox/ShadowMap/" + std::to_string(settings.shadow_map_resolution.value) + "/"
                + std::to_string(draw_data.shadows.layer_count),
            make_shadow_map_desc(
                settings.shadow_map_resolution.value,
                draw_data.shadows.layer_count));
        if (!draw_data.shadows.map.resource.is_valid())
            return Result(false, "Frame pipeline failed: shadow map target upload failed.");

        return {};
    }

    static Result append_shadow_passes(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsSettings& settings,
        const FrameData& frame,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const RenderDrawData& draw_data,
        std::vector<RenderPass>& out_shadow_passes,
        GraphicsResourceBinding& out_lighting_shadow_uniform)
    {
        if (draw_data.shadows.layer_count == 0U)
            return {};

        if (!draw_data.shadows.map.resource.is_valid())
            return Result(false, "Rendering pipeline failed: shadow map resource is invalid.");

        auto shadow_material_upload = RenderingMaterialUploadData();
        auto result = resource_manager.upload_material(
            MaterialInstance(ShadowMapMaterial::HANDLE),
            shadow_material_upload);
        if (!result)
            return result;

        const auto shadow_material_uniform = resource_manager.upload_uniform_buffer(
            BINDING_MATERIAL_DATA,
            "Shadow Material Shader Data",
            "Toybox/Uniforms/Material/ShadowMap",
            frame_index,
            shadow_material_upload.uniform_values.data(),
            static_cast<uint64>(shadow_material_upload.uniform_values.size() * sizeof(Vec4)));
        if (!shadow_material_upload.pipeline.is_valid()
            || !shadow_material_uniform.resource.is_valid())
        {
            return Result(false, "Rendering pipeline failed: shadow material upload failed.");
        }

        const auto frame_shadow_data = make_shadow_shader_data(
            draw_data,
            frame,
            settings.shadow_render_distance.value,
            settings.shadow_softness.value);

        out_shadow_passes.reserve(draw_data.shadows.layer_count);
        for (uint32 layer = 0U; layer < draw_data.shadows.layer_count; ++layer)
        {
            const auto layer_shadow_data =
                make_shadow_shader_data_for_layer(frame_shadow_data, layer);
            const auto shadow_uniform = resource_manager.upload_uniform_buffer(
                BINDING_SHADOW_PASS_DATA,
                "Shadow Shader Data",
                "Toybox/Uniforms/Shadow/" + std::to_string(layer),
                frame_index,
                &layer_shadow_data,
                static_cast<uint64>(sizeof(layer_shadow_data)));
            if (!shadow_uniform.resource.is_valid())
                return Result(false, "Rendering pipeline failed: shadow uniform upload failed.");
            if (layer == 0U)
                out_lighting_shadow_uniform = shadow_uniform;

            auto shadow_pass = RenderPass {
                .desc =
                    GraphicsPassDesc {
                        .depth_stencil_target = draw_data.shadows.map.resource,
                        .depth_stencil_layer = static_cast<int32>(layer),
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .debug_name = "Toybox Shadow Pass",
                    },
            };
            result = append_shadow_draws(
                resource_manager,
                frame_index,
                frame_uniform,
                camera_uniform,
                light_uniform,
                shadow_uniform,
                shadow_material_upload,
                shadow_material_uniform,
                draw_data.shadow_batches,
                shadow_pass);
            if (!result)
                return result;

            out_shadow_passes.push_back(std::move(shadow_pass));
        }

        return {};
    }

    static void append_gbuffer_textures(
        const GBuffer& gbuffer,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        // GBuffer inputs are injected by the pipeline so material fallbacks cannot override the
        // actual frame targets.
        out_textures.push_back(gbuffer.albedo);
        out_textures.push_back(gbuffer.normal);
        out_textures.push_back(gbuffer.material);
        out_textures.push_back(gbuffer.emissive);
        out_textures.push_back(gbuffer.depth);
        out_textures.push_back(gbuffer.final_color);
    }

    static bool should_render_post_effect(const PostProcessingEffect& effect)
    {
        return effect.is_enabled && effect.blend > 0.0F && effect.material.get_handle().is_valid();
    }

    static Result append_post_process_draw(
        RenderingResourceManager& resource_manager,
        const uint64 frame_index,
        const GraphicsResourceBinding& frame_uniform,
        const GraphicsResourceBinding& camera_uniform,
        const GraphicsResourceBinding& light_uniform,
        const GBuffer& gbuffer,
        const MaterialInstance& material,
        const std::string& uniform_cache_key,
        RenderPass& render_pass)
    {
        auto material_upload = RenderingMaterialUploadData();
        auto result = resource_manager.upload_material(material, material_upload);
        if (!result)
            return result;

        const auto material_uniform = resource_manager.upload_uniform_buffer(
            BINDING_MATERIAL_DATA,
            "Post Process Material Shader Data",
            uniform_cache_key,
            frame_index,
            material_upload.uniform_values.data(),
            static_cast<uint64>(material_upload.uniform_values.size() * sizeof(Vec4)));
        if (!material_upload.pipeline.is_valid() || !material_uniform.resource.is_valid())
        {
            return Result(false, "Rendering pipeline failed: post process upload failed.");
        }

        append_gbuffer_textures(gbuffer, material_upload.textures);
        render_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = material_upload.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform, material_uniform},
                .textures = std::move(material_upload.textures),
                .vertex_count = 3U,
            });

        return {};
    }

    static std::vector<RenderPass> create_passes(
        const uint frame_index,
        const FrameData& frame,
        const GBuffer& gbuffer,
        const RenderDrawData& draw_data,
        const GraphicsSettings& settings,
        RenderingResourceManager& resource_manager,
        IGraphicsBackend& backend)
    {
        const auto fail = [&backend](const char* message) -> std::vector<RenderPass>
        {
            TBX_TRACE_ERROR_ONCE("Rendering pipeline pass creation failed: {}", message);
            backend.end_frame();
            return {};
        };
        const auto fail_result =
            [&backend](const char* operation, const Result& result) -> std::vector<RenderPass>
        {
            TBX_TRACE_ERROR_ONCE(
                "Rendering pipeline pass creation failed during {}: {}",
                operation,
                result.get_report());
            backend.end_frame();
            return {};
        };

        // Upload data
        const auto light_shader_data = internal::make_light_shader_data(draw_data);

        const auto frame_shader_data = FrameShaderData {
            .time = frame.time,
            .delta_time = frame.delta_time,
            .viewport_size = Vec2(
                static_cast<float>(frame.viewport.dimensions.width),
                static_cast<float>(frame.viewport.dimensions.height)),
        };
        const auto camera_shader_data = CameraShaderData {
            .view = frame.camera.view,
            .projection = frame.camera.projection,
            .view_projection = frame.camera.view_projection,
            .inverse_view = frame.camera.inverse_view,
            .inverse_projection = frame.camera.inverse_projection,
            .world_position = Vec4(frame.camera.position, 1.0F),
        };

        const auto frame_uniform = resource_manager.upload_uniform_buffer(
            BINDING_FRAME_DATA,
            "Frame Shader Data",
            "Toybox/Uniforms/Frame",
            frame_index,
            &frame_shader_data,
            static_cast<uint64>(sizeof(frame_shader_data)));
        const auto camera_uniform = resource_manager.upload_uniform_buffer(
            BINDING_CAMERA_DATA,
            "Camera Shader Data",
            "Toybox/Uniforms/Camera",
            frame_index,
            &camera_shader_data,
            static_cast<uint64>(sizeof(camera_shader_data)));
        const auto light_uniform = resource_manager.upload_uniform_buffer(
            BINDING_LIGHT_DATA,
            "Light Shader Data",
            "Toybox/Uniforms/Light",
            frame_index,
            &light_shader_data,
            static_cast<uint64>(sizeof(light_shader_data)));
        if (!frame_uniform.resource.is_valid() || !camera_uniform.resource.is_valid()
            || !light_uniform.resource.is_valid())
        {
            return fail("frame, camera, or light uniform upload returned an invalid resource.");
        }

        auto shadow_map = draw_data.shadows.map;
        auto shadow_passes = std::vector<RenderPass>();
        auto lighting_shadow_uniform = GraphicsResourceBinding {.slot = BINDING_SHADOW_PASS_DATA};
        auto result = append_shadow_passes(
            resource_manager,
            frame_index,
            settings,
            frame,
            frame_uniform,
            camera_uniform,
            light_uniform,
            draw_data,
            shadow_passes,
            lighting_shadow_uniform);
        if (!result)
            return fail_result("shadow pass upload", result);

        // Sky Pass: Draws sky geometry into the final color target before scene lighting.
        auto sky_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .clear_color = draw_data.clear_color,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Skybox Pass",
                },
        };
        if (draw_data.sky.material.get_handle().is_valid())
        {
            const auto sky_mesh_handle = draw_data.sky.type == SkyType::BOX
                                             ? Handle("Toybox/SkyBox")
                                             : Handle("Toybox/SkySphere");
            const Mesh& sky_mesh = draw_data.sky.type == SkyType::BOX ? Mesh::CUBE : Mesh::SPHERE;
            auto uploaded_sky_mesh = RenderingMeshUploadData();
            auto result = resource_manager.upload_static_runtime_mesh(
                sky_mesh_handle,
                sky_mesh,
                uploaded_sky_mesh);
            if (!result)
                return fail_result("sky mesh upload", result);

            auto sky_material_upload = RenderingMaterialUploadData();
            result = resource_manager.upload_material(draw_data.sky.material, sky_material_upload);
            if (!result)
                return fail_result("sky material upload", result);
            append_fallback_texture(
                resource_manager,
                PARAM_SKYBOX_TEXTURE,
                sky_material_upload.textures);
            append_fallback_texture(
                resource_manager,
                PARAM_SECONDARY_SKYBOX_TEXTURE,
                sky_material_upload.textures);

            const auto sky_instance = internal::RenderMeshInstance {};
            const auto sky_instance_buffer = resource_manager.upload_instance_buffer(
                "Toybox/Instances/Sky",
                frame_index,
                &sky_instance,
                static_cast<uint64>(sizeof(sky_instance)));
            const auto sky_object_data = ObjectShaderData {
                .model = Mat4(1.0F),
                .normal_matrix = Mat4(1.0F),
            };
            const auto sky_object_uniform = resource_manager.upload_uniform_buffer(
                BINDING_OBJECT_DATA,
                "Object Shader Data",
                "Toybox/Uniforms/Object/Sky",
                frame_index,
                &sky_object_data,
                static_cast<uint64>(sizeof(sky_object_data)));
            const auto sky_material_uniform = resource_manager.upload_uniform_buffer(
                BINDING_MATERIAL_DATA,
                "Material Shader Data",
                "Toybox/Uniforms/Material/Sky",
                frame_index,
                sky_material_upload.uniform_values.data(),
                static_cast<uint64>(sky_material_upload.uniform_values.size() * sizeof(Vec4)));
            if (!sky_instance_buffer.resource.is_valid() || !sky_object_uniform.resource.is_valid()
                || !sky_material_uniform.resource.is_valid())
            {
                return fail(
                    "sky instance, object uniform, or material uniform upload returned an invalid "
                    "resource.");
            }

            sky_pass.indexed_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = sky_material_upload.pipeline,
                    .index_buffer = uploaded_sky_mesh.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = VERTEX_BUFFER_SLOT_MESH,
                                .resource = uploaded_sky_mesh.vertex_buffer,
                            },
                            sky_instance_buffer,
                        },
                    .uniform_buffers =
                        {
                            frame_uniform,
                            camera_uniform,
                            light_uniform,
                            sky_object_uniform,
                            sky_material_uniform,
                        },
                    .textures = sky_material_upload.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = uploaded_sky_mesh.index_count,
                        },
                });
        }

        // Opaque Pass: Draws opaque scene geometry into GBuffer targets and depth.
        auto opaque_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets =
                        {
                            gbuffer.albedo.resource,
                            gbuffer.normal.resource,
                            gbuffer.material.resource,
                            gbuffer.emissive.resource,
                        },
                    .depth_stencil_target = gbuffer.depth.resource,
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox GBuffer Pass",
                },
        };
        result = internal::append_batch_draws(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            nullptr,
            nullptr,
            draw_data.opaque_batches,
            opaque_pass);
        if (!result)
            return fail_result("opaque batch draw upload", result);

        // Alpha Cutout Pass: Reserved for alpha-tested geometry that should write GBuffer targets
        // with depth.
        auto alpha_cutout_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Alpha Cutout Scene Pass",
                },
        };

        // Lighting Pass: Computes lighting from the GBuffer into the final color target.
        const bool has_sky_draws = !sky_pass.draws.empty() || !sky_pass.indexed_draws.empty();
        auto lighting_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .clear_color = draw_data.clear_color,
                    .clear_flags =
                        has_sky_draws ? GraphicsClearFlags::NONE : GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Lighting Pass",
                },
        };
        auto lighting_material = RenderingMaterialUploadData();
        result = resource_manager.upload_material(
            MaterialInstance(tbx::LightingMaterial::HANDLE),
            lighting_material);
        if (!result)
            return fail_result("lighting material upload", result);
        lighting_pass.draws.push_back(
            GraphicsDrawCommand {
                .pipeline = lighting_material.pipeline,
                .uniform_buffers = {frame_uniform, camera_uniform, light_uniform},
                .textures =
                    {
                        gbuffer.albedo,
                        gbuffer.normal,
                        gbuffer.material,
                        gbuffer.emissive,
                        gbuffer.depth,
                    },
                .vertex_count = 3U,
            });
        if (lighting_shadow_uniform.resource.is_valid())
            lighting_pass.draws.back().uniform_buffers.push_back(lighting_shadow_uniform);
        if (shadow_map.resource.is_valid())
            lighting_pass.draws.back().textures.push_back(shadow_map);

        // Transparent Pass: Draws alpha-blended geometry forward over the lit scene color.
        auto transparent_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .color_targets = {gbuffer.final_color.resource},
                    .depth_stencil_target = gbuffer.depth.resource,
                    .clear_flags = GraphicsClearFlags::NONE,
                    .debug_name = "Toybox Transparent Forward Pass",
                },
        };
        result = internal::append_batch_draws(
            resource_manager,
            frame_index,
            frame_uniform,
            camera_uniform,
            light_uniform,
            lighting_shadow_uniform.resource.is_valid() ? &lighting_shadow_uniform : nullptr,
            shadow_map.resource.is_valid() ? &shadow_map : nullptr,
            draw_data.transparent_batches,
            transparent_pass);
        if (!result)
            return fail_result("transparent batch draw upload", result);

        // Post Process Pass: Applies fullscreen post processing from the GBuffer to the current
        // frame target.
        auto post_process_pass = RenderPass {
            .desc =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_flags = GraphicsClearFlags::COLOR,
                    .debug_name = "Toybox Post Process Pass",
                },
        };
        if (draw_data.post_processing.is_enabled)
        {
            for (const auto& effect : draw_data.post_processing.effects)
            {
                if (!should_render_post_effect(effect))
                    continue;

                auto effect_material = effect.material;
                effect_material.set_float(PARAM_BLEND, effect.blend);
                result = append_post_process_draw(
                    resource_manager,
                    frame_index,
                    frame_uniform,
                    camera_uniform,
                    light_uniform,
                    gbuffer,
                    effect_material,
                    std::string("Toybox/Uniforms/Material/PostEffect/")
                        + std::to_string(hash(effect_material)),
                    post_process_pass);
                if (!result)
                    return fail_result("post process effect upload", result);
            }
        }

        // Create pass list and return
        auto passes = std::vector<RenderPass>();
        passes.reserve(6U + shadow_passes.size());
        for (auto& shadow_pass : shadow_passes)
            passes.push_back(std::move(shadow_pass));
        if (has_sky_draws)
            passes.push_back(std::move(sky_pass));
        passes.push_back(std::move(opaque_pass));
        if (!alpha_cutout_pass.draws.empty() || !alpha_cutout_pass.indexed_draws.empty())
            passes.push_back(std::move(alpha_cutout_pass));
        passes.push_back(std::move(lighting_pass));
        if (!transparent_pass.draws.empty() || !transparent_pass.indexed_draws.empty())
            passes.push_back(std::move(transparent_pass));
        if (!post_process_pass.draws.empty() || !post_process_pass.indexed_draws.empty())
            passes.push_back(std::move(post_process_pass));
        return passes;
    }

    static RenderDrawData create_draw_data(
        const EntityRegistry& entity_registry,
        const Vec3& camera_position,
        AssetManager& asset_manager,
        const float local_light_max_distance,
        const float shadow_caster_max_distance)
    {
        auto draw_data = RenderDrawData();

        for (auto& entity : entity_registry.get_with<Sky>())
        {
            draw_data.sky = entity.get_component<Sky>();
            if (!draw_data.sky.material.get_handle().is_valid())
                draw_data.sky.material = MaterialInstance(TexturedSkyMaterial::HANDLE);

            const Color clear_color =
                draw_data.sky.material.get_parameter_or(TexturedSkyMaterial::COLOR, Color::BLACK);
            draw_data.clear_color = clear_color;

            // TODO: Warn if there is more than one sky, and say picking first found as only one is
            // supported.
            break;
        }

        for (auto& entity : entity_registry.get_with<PostProcessing>())
        {
            draw_data.post_processing = entity.get_component<PostProcessing>();
            break;
        }

        for (auto& entity : entity_registry.get_with<DirectionalLight>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_optional_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index = reserve_shadow_index(
                draw_data.lighting.shadow_layer_count,
                light.cast_shadows,
                DIRECTIONAL_SHADOW_CASCADE_COUNT);

            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_DIRECTIONAL),
                    .color = light.color,
                    .intensity = light.intensity,
                    .ambient = light.ambient,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? DIRECTIONAL_SHADOW_CASCADE_COUNT : 0U,
                    .direction = direction,
                });
        }

        for (auto& entity : entity_registry.get_with<PointLight>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_POINT),
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        for (auto& entity : entity_registry.get_with<SpotLight>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type =
                        static_cast<uint>(SHADER_LIGHT_TYPE_SPOT), // TODO: Convert types to ints
                    .color = light.color,
                    .intensity = light.intensity,
                    .inner_cone = angle_to_cosine(light.inner_angle),
                    .outer_cone = angle_to_cosine(light.outer_angle),
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .direction = direction,
                    .range = light.range,
                });
        }

        for (auto& entity : entity_registry.get_with<AreaLight>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_optional_transform(entity);
            if (!is_within_local_light_range(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
                continue;

            const int32 shadow_index =
                reserve_shadow_index(draw_data.lighting.shadow_layer_count, light.cast_shadows, 1U);
            append_light(
                draw_data,
                RenderLight {
                    .type = static_cast<uint>(SHADER_LIGHT_TYPE_POINT),
                    .color = light.color,
                    .intensity = light.intensity,
                    .shadow_index = shadow_index,
                    .shadow_layer_count = shadow_index >= 0 ? 1U : 0U,
                    .position = transform.position,
                    .range = light.range,
                });
        }

        draw_data.shadows.layer_count = draw_data.lighting.shadow_layer_count;

        for (auto& entity : entity_registry.get_with<StaticMesh>())
        {
            const auto& static_mesh = entity.get_component<StaticMesh>();
            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config = resolve_draw_material_config(asset_manager, material);
            const uint64 material_key = hash(material);
            const Transform transform = get_optional_transform(entity);
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = static_mesh.handle,
                .data = static_mesh,
            };
            auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                ? draw_data.transparent_batches
                                : draw_data.opaque_batches;
            append_batch(
                batches,
                make_batch_hash(static_mesh.handle.get_id(), nullptr, material_key),
                mesh,
                material,
                RenderMeshInstance {
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                });
            if (draw_data.shadows.layer_count > 0U
                && should_cast_shadow(
                    config,
                    transform.position,
                    camera_position,
                    shadow_caster_max_distance))
            {
                append_batch(
                    draw_data.shadow_batches,
                    make_batch_hash(static_mesh.handle.get_id(), nullptr, 0U),
                    mesh,
                    MaterialInstance(),
                    RenderMeshInstance {
                        .model_matrix = model_matrix,
                        .normal_matrix = normal(model_matrix),
                    });
            }
        }

        for (auto& entity : entity_registry.get_with<DynamicMesh>())
        {
            const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
            const auto mesh_data = dynamic_mesh.get_data();
            const MaterialInstance material = entity.has_component<MaterialInstance>()
                                                  ? entity.get_component<MaterialInstance>()
                                                  : MaterialInstance();
            const MaterialConfig config = resolve_draw_material_config(asset_manager, material);
            const uint64 material_key = hash(material);
            const Transform transform = get_optional_transform(entity);
            const Mat4 model_matrix = build_transform_matrix(transform);
            const auto mesh = RenderMesh {
                .handle = Handle(
                    "Toybox/DynamicMesh_"
                    + std::to_string(std::hash<DynamicMeshData*> {}(mesh_data.get()))),
                .data = dynamic_mesh,
            };
            auto& batches = config.blend_mode == MaterialBlendMode::ALPHA_BLEND
                                ? draw_data.transparent_batches
                                : draw_data.opaque_batches;
            append_batch(
                batches,
                make_batch_hash(
                    Uuid(static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get()))),
                    mesh_data.get(),
                    material_key),
                mesh,
                material,
                RenderMeshInstance {
                    .model_matrix = model_matrix,
                    .normal_matrix = normal(model_matrix),
                });
            if (draw_data.shadows.layer_count > 0U
                && should_cast_shadow(
                    config,
                    transform.position,
                    camera_position,
                    shadow_caster_max_distance))
            {
                append_batch(
                    draw_data.shadow_batches,
                    make_batch_hash(
                        Uuid(
                            static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get()))),
                        mesh_data.get(),
                        0U),
                    mesh,
                    MaterialInstance(),
                    RenderMeshInstance {
                        .model_matrix = model_matrix,
                        .normal_matrix = normal(model_matrix),
                    });
            }
        }

        return draw_data;
    }

    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsDrawCommand& command)
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw(command.vertex_count, command.vertex_offset);
    }

    static Result execute_draw_command(
        IGraphicsBackend& backend,
        const GraphicsIndexedDrawCommand& command)
    {
        auto result = backend.bind_pipeline(command.pipeline);
        if (!result)
            return result;

        for (const auto& binding : command.vertex_buffers)
        {
            result = backend.bind_vertex_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        result = backend.bind_index_buffer(command.index_buffer, command.index_type);
        if (!result)
            return result;

        for (const auto& binding : command.uniform_buffers)
        {
            result = backend.bind_uniform_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.storage_buffers)
        {
            result = backend.bind_storage_buffer(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.textures)
        {
            result = backend.bind_texture(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        for (const auto& binding : command.samplers)
        {
            result = backend.bind_sampler(binding.slot, binding.resource);
            if (!result)
                return result;
        }

        return backend.draw_indexed(command.draw);
    }

    static Result execute_passes(
        IGraphicsBackend& backend,
        const std::vector<RenderPass>& render_passes)
    {
        for (const auto& render_pass : render_passes)
        {
            auto result = backend.begin_pass(render_pass.desc);
            if (!result)
                return result;

            for (const auto& draw_command : render_pass.draws)
            {
                result = execute_draw_command(backend, draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            for (const auto& indexed_draw_command : render_pass.indexed_draws)
            {
                result = execute_draw_command(backend, indexed_draw_command);
                if (!result)
                {
                    (void)backend.end_pass();
                    return result;
                }
            }

            result = backend.end_pass();
            if (!result)
                return result;
        }

        return {};
    }

    static FrameData create_frame_data(
        const uint frame,
        const DeltaTime delta_time,
        const float elapsed_time,
        const Size resolution,
        const EntityRegistry& entity_registry,
        const IWindowManager& window_manager)
    {
        auto render_camera = Camera();
        auto cam_transform = Transform();
        for (auto& entity : entity_registry.get_with<Camera>())
        {
            render_camera = entity.get_component<Camera>();
            // Cameras without a Transform render from identity so authoring transform-less test
            // scenes still produces deterministic frame data.
            if (entity.has_component<Transform>())
                cam_transform = get_world_space_transform(entity);
            break;
        }

        auto render_target = window_manager.get_main_window();
        auto cam_target = render_camera.get_render_target();
        if (cam_target.is_valid())
            render_target = cam_target;

        auto target_resolution = window_manager.get_size(render_target);
        if (target_resolution.width == 0U || target_resolution.height == 0U)
            target_resolution = Size {1U, 1U};

        auto render_resolution = resolution;
        if (render_resolution.width == 0U || render_resolution.height == 0U)
            render_resolution = target_resolution;

        auto render_viewport = Viewport {Vec2(0.0F), render_resolution};
        auto cam_viewport = render_camera.get_viewport();
        if (!cam_viewport.is_zero())
            render_viewport = cam_viewport;

        render_camera.set_aspect(render_resolution.get_aspect_ratio());
        const Mat4 cam_view_matrix =
            render_camera.get_view_matrix(cam_transform.position, cam_transform.rotation);
        const Mat4 cam_projection_matrix = render_camera.get_projection_matrix();
        const Mat4 cam_view_projection_matrix = cam_projection_matrix * cam_view_matrix;

        return FrameData {
            .index = frame,
            .time = elapsed_time,
            .delta_time = static_cast<float>(delta_time.seconds),
            .resolution = render_resolution,
            .viewport = render_viewport,
            .target = render_target,
            .camera =
                RenderCamera {
                    .view = cam_view_matrix,
                    .position = cam_transform.position,
                    .projection = cam_projection_matrix,
                    .view_projection = cam_view_projection_matrix,
                    .inverse_view = inverse(cam_view_matrix),
                    .inverse_projection = inverse(cam_projection_matrix),
                },
        };
    }

    static GraphicsTextureDesc make_gbuffer_color_target_desc(
        const Size& render_resolution,
        const GraphicsTextureFormat format,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
            .format = format,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static GraphicsTextureDesc make_gbuffer_depth_target_desc(
        const Size& render_resolution,
        const std::string& debug_name)
    {
        return GraphicsTextureDesc {
            .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
            .format = GraphicsTextureFormat::DEPTH32_FLOAT,
            .size = render_resolution,
            .mip_count = 1U,
            .array_layer_count = 1U,
            .debug_name = debug_name,
        };
    }

    static Result create_gbuffer(
        RenderingResourceManager& resource_manager,
        const FrameData& frame,
        GBuffer& out_buff)
    {
        const Size viewport_size = frame.viewport.dimensions;
        const std::string render_target_key =
            std::to_string(viewport_size.width) + "x" + std::to_string(viewport_size.height);

        out_buff.albedo = resource_manager.upload_texture(
            BINDING_GBUFFER_ALBEDO,
            "Toybox/GBuffer/Albedo/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Albedo"));
        out_buff.normal = resource_manager.upload_texture(
            BINDING_GBUFFER_NORMAL,
            "Toybox/GBuffer/Normal/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Normal"));
        out_buff.material = resource_manager.upload_texture(
            BINDING_GBUFFER_MATERIAL,
            "Toybox/GBuffer/Material/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Material"));
        out_buff.emissive = resource_manager.upload_texture(
            BINDING_GBUFFER_EMISSIVE,
            "Toybox/GBuffer/Emissive/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Emissive"));
        out_buff.depth = resource_manager.upload_texture(
            BINDING_GBUFFER_DEPTH,
            "Toybox/GBuffer/Depth/" + render_target_key,
            make_gbuffer_depth_target_desc(viewport_size, "Toybox GBuffer Depth"));
        out_buff.final_color = resource_manager.upload_texture(
            BINDING_GBUFFER_FINAL_COLOR,
            "Toybox/FinalColor/" + render_target_key,
            make_gbuffer_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox Final Color"));

        if (!out_buff.albedo.resource.is_valid() || !out_buff.normal.resource.is_valid()
            || !out_buff.material.resource.is_valid() || !out_buff.emissive.resource.is_valid()
            || !out_buff.depth.resource.is_valid() || !out_buff.final_color.resource.is_valid())
        {
            return Result(false, "Frame pipeline failed: GBuffer target upload failed.");
        }

        return {};
    }
}
