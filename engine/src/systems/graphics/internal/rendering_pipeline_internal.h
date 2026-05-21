#pragma once
#include "systems/graphics/internal/render_metrics_internal.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/shader_bindings.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/light.h"
#include "tbx/types/components/material_instance.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/components/sky.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/matrices.h"
#include "tbx/types/trig.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace tbx::internal
{
    using CascadeSplitCollection = std::array<Vec2, DIRECTIONAL_SHADOW_CASCADE_COUNT>;
    using FrustumCornerCollection = std::array<Vec3, 8U>;

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
        Vec3 position = Vec4(0.0F, 0.0F, 0.0F, 1.0F);
        Mat4 projection = Mat4(1.0F);
        Mat4 view_projection = Mat4(1.0F);
        Mat4 inverse_view = Mat4(1.0F);
        Mat4 inverse_projection = Mat4(1.0F);
    };

    struct RenderLights
    {
        unit4 type = 0;
        Color color;
        uint intensity;
        uint inner_cone;
        uint outter_cone;
        uint shadow_index;
        uint shodow_layer_count;
        Vec3 position = {};
        Vec3 direction = {};
    };

    struct ShadowCascades
    {
        uint32 count = 0U;
        GraphicsResourceBinding map = {};
        std::vector<GraphicsResourceBinding> buffers = {};
    };

    struct RenderMesh
    {
        using Data = std::variant<StaticMesh, DynamicMesh>;

        Handle handle;
        Data data;
    };

    /// @brief
    /// Purpose: Stores one render instance transform for an indexed draw command.
    struct RenderMeshInstance
    {
        Mat4 model_matrix = Mat4(1.0F);
        Mat4 normal_matrix = Mat4(1.0F);
    };

    struct RenderBatch
    {
        uint64 hash; // <- obtained via hash_render_batch used to cache batch
        Uuid pipeline;
        RenderMesh mesh;
        std::vector<RenderMeshInstance> instances = {};
    };

    struct FrameData
    {
        uint index = 0;
        float time = 0;
        float delta_time = 0;
        Size resolution = {};
        Viewport viewport = {};
        RenderTarget target = {};
        RenderCamera camera = {};
        GBuffer g_buffer = {};
    };

    struct RenderDrawData
    {
        Sky sky;
        ShadowCascades shadows;
        RenderBatch opaque_batches = {};
        RenderBatch transparent_batches = {};
        RenderBatch shadow_batches = {};
        std::array<RenderLight, MAX_LIGHTS> lights;
    };

    static uint64 hash_render_batch(
        const RenderingMeshSourceType mesh_source,
        const Uuid& mesh_id,
        const DynamicMeshData* dynamic_mesh,
        const uint64 material_key)
    {
        uint64 result = hash(static_cast<uint64>(mesh_source), TBX_FNV1A_OFFSET_BASIS);
        result = hash(mesh_id, result);
        result = hash(static_cast<uint64>(reinterpret_cast<std::uintptr_t>(dynamic_mesh)), result);
        result = hash(material_key, result);
        return result == 0U ? 1U : result;
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

    static Result execute(IGraphicsBackend& backend, const std::vector<RenderPass>& render_passes)
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

    FrameData create_frame_data(
        const uint frame,
        const DeltaTime delta_time,
        const float elapsed_time,
        const Size resolution,
        const EntityRegistry& entity_registry,
        const IWindowManager& window_manager)
    {
        static float s_elapsed_time = 0;

        auto render_camera = Camera();
        auto cam_transform = Transform();
        for (auto& entity : entity_registry.get_with<Camera, Transform>())
        {
            render_camera = entity.get_component<Camera>();
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

        Viewport render_viewport = {{0, 0}, render_resolution};
        auto cam_viewport = render_camera.get_viewport();
        if (cam_viewport.is_zero())
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

    static std::string make_batch_debug_name(const uint64 batch_key, const std::string& prefix)
    {
        return prefix + std::to_string(batch_key);
    }

    static MaterialInstance make_sky_material_instance(const Sky& sky)
    {
        auto material = sky.material;
        if (!material.get_handle().is_valid())
            material.material = TexturedSkyMaterial::HANDLE;

        return material;
    }

    static const Handle& get_sky_mesh_handle(const SkyType type)
    {
        static const auto box_handle = Handle("Toybox/SkyBox");
        static const auto sphere_handle = Handle("Toybox/SkySphere");
        return type == SkyType::BOX ? box_handle : sphere_handle;
    }

    static const Mesh& get_sky_mesh(const SkyType type)
    {
        return type == SkyType::BOX ? Mesh::CUBE : Mesh::SPHERE;
    }

    static Vec3 make_light_color(const Light& light)
    {
        return Vec3(light.color.r, light.color.g, light.color.b);
    }

    static bool is_within_distance_limit(
        const Vec3& source,
        const Vec3& target,
        const float max_distance)
    {
        return max_distance <= 0.0F || distance(source, target) <= max_distance;
    }

    static Vec3 make_shadow_up_vector(const Vec3& direction)
    {
        return std::abs(dot(direction, Vec3(0.0F, 1.0F, 0.0F))) > 0.95F ? Vec3(0.0F, 0.0F, 1.0F)
                                                                        : Vec3(0.0F, 1.0F, 0.0F);
    }

    static float reserve_shadow_index(
        int32& shadow_count,
        const bool casts_shadows,
        const uint32 shadow_layer_count)
    {
        if (!casts_shadows || shadow_layer_count == 0U
            || static_cast<uint32>(shadow_count) + shadow_layer_count > MAX_LIGHTS)
        {
            return -1.0F;
        }

        const float shadow_index = static_cast<float>(shadow_count);
        shadow_count += static_cast<int32>(shadow_layer_count);
        return shadow_index;
    }

    static float make_shadow_layer_count(const float shadow_index, const uint32 layer_count)
    {
        return shadow_index >= 0.0F ? static_cast<float>(layer_count) : 0.0F;
    }

    static void append_shadow_caster(
        ShadowPassShaderData& shadow_data,
        const Mat4& light_view_projection,
        const Vec4& light_direction,
        const float shadow_depth_bias,
        const float shadow_normal_bias,
        const float shadow_strength,
        const float shadow_slope_bias,
        const Vec4& shadow_extra_params)
    {
        const int32 shadow_index = shadow_data.shadow_meta.x;
        if (shadow_index < 0 || static_cast<uint32>(shadow_index) >= MAX_LIGHTS)
            return;

        shadow_data.light_view_projections[static_cast<size>(shadow_index)] = light_view_projection;
        shadow_data.light_directions[static_cast<size>(shadow_index)] = light_direction;
        shadow_data.shadow_params[static_cast<size>(shadow_index)] =
            Vec4(shadow_depth_bias, shadow_normal_bias, shadow_strength, shadow_slope_bias);
        shadow_data.shadow_extra_params[static_cast<size>(shadow_index)] = shadow_extra_params;
        shadow_data.shadow_meta.x = shadow_index + 1;
    }

    static void append_shader_light(
        LightShaderData& light_data,
        const ShaderLightData& shader_light)
    {
        const int32 light_count = light_data.light_meta.x;
        if (light_count < 0 || static_cast<uint32>(light_count) >= MAX_LIGHTS)
            return;

        light_data.lights[static_cast<size>(light_count)] = shader_light;
        light_data.light_meta.x = light_count + 1;
    }

    static LightShaderData build_light_shader_data(
        EntityRegistry& entity_registry,
        const Vec3& camera_position,
        const float local_light_max_distance)
    {
        auto light_data = LightShaderData();
        auto ambient_color_sum = Vec3(0.0F);
        auto ambient_intensity_sum = 0.0F;
        auto directional_light_count = 0U;
        auto shadow_count = int32 {};

        for (auto& entity : entity_registry.get_with<DirectionalLight, Transform>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_world_space_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const Vec3 color = make_light_color(light);
            const float shadow_index = reserve_shadow_index(
                shadow_count,
                light.cast_shadows,
                DIRECTIONAL_SHADOW_CASCADE_COUNT);

            ambient_color_sum += color;
            ambient_intensity_sum += light.ambient;
            ++directional_light_count;

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(0.0F, 0.0F, 0.0F, SHADER_LIGHT_TYPE_DIRECTIONAL),
                    .direction_range = Vec4(direction, 0.0F),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        0.0F,
                        0.0F,
                        shadow_index,
                        make_shadow_layer_count(shadow_index, DIRECTIONAL_SHADOW_CASCADE_COUNT)),
                });
        }

        for (auto& entity : entity_registry.get_with<PointLight, Transform>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            const Vec3 color = make_light_color(light);
            const float shadow_index = reserve_shadow_index(shadow_count, light.cast_shadows, 1U);

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_POINT),
                    .direction_range = Vec4(0.0F, 0.0F, 0.0F, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params =
                        Vec4(0.0F, 0.0F, shadow_index, make_shadow_layer_count(shadow_index, 1U)),
                });
        }

        for (auto& entity : entity_registry.get_with<SpotLight, Transform>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const Vec3 color = make_light_color(light);
            const float shadow_index = reserve_shadow_index(shadow_count, light.cast_shadows, 1U);

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_SPOT),
                    .direction_range = Vec4(direction, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        angle_to_cosine(light.inner_angle),
                        angle_to_cosine(light.outer_angle),
                        shadow_index,
                        make_shadow_layer_count(shadow_index, 1U)),
                });
        }

        for (auto& entity : entity_registry.get_with<AreaLight, Transform>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            const Vec3 color = make_light_color(light);
            const float shadow_index = reserve_shadow_index(shadow_count, light.cast_shadows, 1U);
            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_POINT),
                    .direction_range = Vec4(0.0F, 0.0F, 0.0F, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params =
                        Vec4(0.0F, 0.0F, shadow_index, make_shadow_layer_count(shadow_index, 1U)),
                });
        }

        light_data.light_meta.y = shadow_count;
        if (directional_light_count > 0U)
        {
            const Vec3 ambient_color =
                ambient_color_sum * (1.0F / static_cast<float>(directional_light_count));
            light_data.ambient_color = Vec4(ambient_color * ambient_intensity_sum, 1.0F);
        }

        return light_data;
    }

    static CascadeSplitCollection build_directional_shadow_cascade_splits(
        const Camera& camera,
        const float shadow_distance)
    {
        auto splits = CascadeSplitCollection {};
        const float near_plane = std::max(camera.get_z_near(), 0.1F);
        const float far_plane =
            std::max(near_plane + 1.0F, std::min(camera.get_z_far(), shadow_distance));
        const float split_lambda = 0.6F;
        auto previous_split = near_plane;

        for (uint32 index = 0U; index < DIRECTIONAL_SHADOW_CASCADE_COUNT; ++index)
        {
            const float split_progress = static_cast<float>(index + 1U)
                                         / static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT);
            const float uniform_split = near_plane + (far_plane - near_plane) * split_progress;
            const float logarithmic_split =
                near_plane * std::pow(far_plane / near_plane, split_progress);
            const float split_distance =
                uniform_split * (1.0F - split_lambda) + logarithmic_split * split_lambda;

            splits[index] = Vec2(previous_split, split_distance);
            previous_split = split_distance;
        }

        splits[DIRECTIONAL_SHADOW_CASCADE_COUNT - 1U].y = far_plane;
        return splits;
    }

    static FrustumCornerCollection build_frustum_corners(
        const Camera& camera,
        const Transform& camera_transform,
        const float split_near,
        const float split_far)
    {
        const Vec3 camera_position = camera_transform.position;
        const Vec3 forward = normalize_or_zero(camera_transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
        const Vec3 right = normalize_or_zero(camera_transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
        const Vec3 up = normalize_or_zero(camera_transform.rotation * Vec3(0.0F, 1.0F, 0.0F));

        auto corners = FrustumCornerCollection {};
        const auto append_plane_corners =
            [&camera, &camera_position, &forward, &right, &up, &corners](
                const float distance_from_camera,
                const uint32 corner_offset)
        {
            float half_height = camera.get_fov() * 0.5F;
            if (camera.is_perspective())
            {
                half_height = std::tan(to_radians(camera.get_fov()) * 0.5F) * distance_from_camera;
            }

            const float half_width = half_height * camera.get_aspect();
            const Vec3 center = camera_position + forward * distance_from_camera;
            corners[corner_offset] = center - right * half_width - up * half_height;
            corners[corner_offset + 1U] = center + right * half_width - up * half_height;
            corners[corner_offset + 2U] = center + right * half_width + up * half_height;
            corners[corner_offset + 3U] = center - right * half_width + up * half_height;
        };

        append_plane_corners(split_near, 0U);
        append_plane_corners(split_far, 4U);
        return corners;
    }

    static Mat4 build_directional_shadow_view_projection(
        const FrustumCornerCollection& corners,
        const Vec3& direction,
        const uint32 shadow_map_resolution)
    {
        auto center = Vec3(0.0F);
        for (const Vec3& corner : corners)
            center += corner;

        center *= 1.0F / static_cast<float>(corners.size());

        auto radius = 0.0F;
        for (const Vec3& corner : corners)
        {
            const Vec3 offset = corner - center;
            radius = std::max(radius, std::sqrt(dot(offset, offset)));
        }

        const float half_extent = std::max(radius * 1.05F, 1.0F);
        const float light_distance = half_extent * 2.0F;
        const Vec3 light_position = center - direction * light_distance;
        const Mat4 view = look_at(light_position, center, make_shadow_up_vector(direction));
        Vec3 min_bounds = Vec3(view * Vec4(corners[0U], 1.0F));
        Vec3 max_bounds = min_bounds;

        for (const Vec3& corner : corners)
        {
            const Vec3 light_space_corner = Vec3(view * Vec4(corner, 1.0F));
            min_bounds.x = std::min(min_bounds.x, light_space_corner.x);
            min_bounds.y = std::min(min_bounds.y, light_space_corner.y);
            min_bounds.z = std::min(min_bounds.z, light_space_corner.z);
            max_bounds.x = std::max(max_bounds.x, light_space_corner.x);
            max_bounds.y = std::max(max_bounds.y, light_space_corner.y);
            max_bounds.z = std::max(max_bounds.z, light_space_corner.z);
        }

        const float bounds_width = max_bounds.x - min_bounds.x;
        const float bounds_height = max_bounds.y - min_bounds.y;
        const float snapped_half_extent =
            std::max(std::max(bounds_width, bounds_height) * 0.525F, half_extent);
        const float texel_size =
            (snapped_half_extent * 2.0F) / static_cast<float>(std::max(shadow_map_resolution, 1U));
        const Vec2 bounds_center =
            Vec2((min_bounds.x + max_bounds.x) * 0.5F, (min_bounds.y + max_bounds.y) * 0.5F);
        const Vec2 snapped_center = Vec2(
            std::floor(bounds_center.x / texel_size) * texel_size,
            std::floor(bounds_center.y / texel_size) * texel_size);
        const float z_margin = std::max(snapped_half_extent * 0.5F, 2.0F);
        const float z_near = std::max(0.1F, -max_bounds.z - z_margin);
        const float z_far = std::max(z_near + 1.0F, -min_bounds.z + z_margin);
        const Mat4 projection = ortho_projection(
            snapped_center.x - snapped_half_extent,
            snapped_center.x + snapped_half_extent,
            snapped_center.y - snapped_half_extent,
            snapped_center.y + snapped_half_extent,
            z_near,
            z_far);

        return projection * view;
    }

    static ShadowCascades build_shadow_shader_data(
        EntityRegistry& entity_registry,
        const Camera& camera,
        const Transform& camera_transform,
        const float shadow_render_distance,
        const float shadow_softness,
        const float local_light_max_distance,
        const uint32 shadow_map_resolution)
    {
        auto shadow_data = ShadowCascades();
        const float shadow_distance = std::max(shadow_render_distance, 1.0F);
        const Vec3 camera_position = camera_transform.position;
        const auto cascade_splits =
            build_directional_shadow_cascade_splits(camera, shadow_distance);

        for (auto& entity : entity_registry.get_with<DirectionalLight, Transform>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            if (!light.cast_shadows
                || static_cast<uint32>(shadow_data.shadow_meta.x) + DIRECTIONAL_SHADOW_CASCADE_COUNT
                       > MAX_LIGHTS)
            {
                continue;
            }

            const Transform transform = get_world_space_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            for (uint32 cascade_index = 0U; cascade_index < DIRECTIONAL_SHADOW_CASCADE_COUNT;
                 ++cascade_index)
            {
                const Vec2 split = cascade_splits[cascade_index];
                const float blend_width = std::min((split.y - split.x) * 0.15F, 8.0F);
                const float blend_start = cascade_index + 1U < DIRECTIONAL_SHADOW_CASCADE_COUNT
                                              ? std::max(split.x, split.y - blend_width)
                                              : split.y;
                const auto corners =
                    build_frustum_corners(camera, camera_transform, split.x, split.y);

                append_shadow_caster(
                    shadow_data,
                    build_directional_shadow_view_projection(
                        corners,
                        direction,
                        shadow_map_resolution),
                    Vec4(direction, 0.0F),
                    0.002F,
                    0.04F,
                    0.75F,
                    0.003F,
                    Vec4(
                        split.x,
                        split.y,
                        blend_start,
                        static_cast<float>(DIRECTIONAL_SHADOW_CASCADE_COUNT)));
            }
        }

        for (auto& entity : entity_registry.get_with<PointLight, Transform>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!light.cast_shadows
                || !is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            Vec3 direction = normalize_or_zero(camera_position - transform.position);
            if (dot(direction, direction) <= 0.0001F)
                direction = Vec3(0.0F, -1.0F, 0.0F);

            const float range = std::max(light.range, 1.0F);
            const Mat4 view = look_at(
                transform.position,
                transform.position + direction,
                make_shadow_up_vector(direction));
            const Mat4 projection = perspective_projection(to_radians(90.0F), 1.0F, 0.1F, range);

            append_shadow_caster(
                shadow_data,
                projection * view,
                Vec4(direction, 0.0F),
                0.003F,
                0.04F,
                0.85F,
                0.005F,
                Vec4(0.0F, range, range, 1.0F));
            (void)shadow_softness;
        }

        for (auto& entity : entity_registry.get_with<SpotLight, Transform>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!light.cast_shadows
                || !is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const float range = std::max(light.range, 1.0F);
            const Mat4 view = look_at(
                transform.position,
                transform.position + direction,
                make_shadow_up_vector(direction));
            const Mat4 projection = perspective_projection(
                to_radians(std::max(light.outer_angle * 2.0F, 1.0F)),
                1.0F,
                0.1F,
                range);

            append_shadow_caster(
                shadow_data,
                projection * view,
                Vec4(direction, 0.0F),
                0.003F,
                0.04F,
                0.85F,
                0.005F,
                Vec4(0.0F, range, range, 1.0F));
            (void)shadow_softness;
        }

        for (auto& entity : entity_registry.get_with<AreaLight, Transform>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_world_space_transform(entity);
            if (!light.cast_shadows
                || !is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                continue;
            }

            const Vec3 direction = Vec3(0.0F, -1.0F, 0.0F);
            const float range = std::max(light.range, 1.0F);
            const Mat4 view = look_at(
                transform.position,
                transform.position + direction,
                make_shadow_up_vector(direction));
            const Mat4 projection =
                ortho_projection(-range, range, -range, range, 0.1F, range * 2.0F);

            append_shadow_caster(
                shadow_data,
                projection * view,
                Vec4(direction, 0.0F),
                0.003F,
                0.04F,
                0.85F,
                0.005F,
                Vec4(0.0F, range * 2.0F, range * 2.0F, 1.0F));
            (void)shadow_softness;
        }

        return shadow_data;
    }

    static const MaterialRenderClassification& resolve_material_render_classification(
        const MaterialInstance& material,
        const uint64 material_key,
        const ResourceUploader& resource_uploader,
        MaterialRenderClassificationCache& cache)
    {
        const auto cached_classification = cache.classifications.find(material_key);
        if (cached_classification != cache.classifications.end())
        {
#if defined(TBX_ENABLE_VERBOSE)
            if (active_render_metrics)
                ++active_render_metrics->material_config_cache_hit_count;
#endif
            return cached_classification->second;
        }

        const MaterialConfig config = resource_uploader.get_material_config(material);
#if defined(TBX_ENABLE_VERBOSE)
        if (active_render_metrics)
            ++active_render_metrics->material_config_cache_miss_count;
#endif
        const auto [classification, _] = cache.classifications.emplace(
            material_key,
            MaterialRenderClassification {
                .config = config,
                .is_transparent = config.blend_mode == MaterialBlendMode::ALPHA_BLEND,
            });
        return classification->second;
    }

    static bool should_material_cast_shadows(
        const MaterialRenderClassification& classification,
        const Vec3& position,
        const Vec3& camera_position,
        const float shadow_caster_max_distance)
    {
        if (classification.config.shadow_mode == ShadowMode::NONE)
            return false;
        if (classification.config.shadow_mode == ShadowMode::ALWAYS)
            return true;

        return is_within_distance_limit(position, camera_position, shadow_caster_max_distance);
    }

    static bool should_material_use_transparent_pass(
        const MaterialRenderClassification& classification)
    {
        return classification.is_transparent;
    }

    static void append_render_batch_instance(
        RenderBatchCollection& batches,
        const uint64 batch_key,
        const RenderingMeshSourceType mesh_source,
        const uint64 material_key,
        const Handle& mesh_handle,
        const std::shared_ptr<DynamicMeshData>& dynamic_mesh,
        const MaterialInstance& material,
        const RenderingDrawInstanceData& instance)
    {
        auto& batch = batches[batch_key];
        if (batch.instances.empty())
        {
            batch.batch_key = batch_key;
            batch.mesh_source = mesh_source;
            batch.material_key = material_key;
            batch.mesh_handle = mesh_handle;
            batch.dynamic_mesh = dynamic_mesh;
            batch.material = material;
        }

        batch.instances.push_back(instance);
    }

    static bool has_draw_commands(const RenderPass& render_pass)
    {
        return !render_pass.draws.empty() || !render_pass.indexed_draws.empty();
    }

    static MaterialInstance make_forward_material(MaterialInstance material)
    {
        const Handle& handle = material.get_handle();
        if (!handle.is_valid() || handle.get_id() == PbrMaterial::HANDLE.get_id()
            || handle.get_name() == "Materials/Pbr.mat")
        {
            material.material = Handle("Materials/ForwardPbr.mat");
        }

        return material;
    }

    static GraphicsTextureDesc make_deferred_color_target_desc(
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

    static GraphicsTextureDesc make_deferred_depth_target_desc(
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

    static Result upload_frame_uniform_buffers(
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        const uint64 frame_index,
        const FrameShaderData& frame_shader_data,
        const CameraShaderData& camera_shader_data,
        const LightShaderData& light_shader_data,
        FrameUniformBindings& out_uniforms)
    {
        // Every pass reads the same frame, camera, and light constants. Upload them once and
        // pass the bindings through each later pipeline stage.
        out_uniforms.buffers =

            if (!out_uniforms.buffers[0U].resource.is_valid()
                || !out_uniforms.buffers[1U].resource.is_valid()
                || !out_uniforms.buffers[2U].resource.is_valid())
        {
            return Result(false, "Frame pipeline factory failed: frame uniform upload failed.");
        }

        return {};
    }

    static Result create_shadow_maps(
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        const uint64 frame_index,
        const uint32 shadow_resolution,
        const uint32 shadow_cascades,
        std::array<Vec4, MAX_LIGHTS>& shadow_params,
        std::array<Vec4, MAX_LIGHTS>& shadow_extra_param,
        std::array<Mat4, MAX_LIGHTS>& light_view_projections,
        std::array<Vec4, MAX_LIGHTS>& light_directions,
        ShadowCascades& out_maps)
    {
        // Shadow passes render first so the lighting and transparent passes can sample the shadow
        // map. A single dummy shadow uniform is still uploaded when no light casts shadows because
        // downstream materials expect the binding slot to exist.
        const bool has_shadowed_light = out_maps.count > 0U;
        const uint32 shadow_layer_count = has_shadowed_light ? shadow_cascades : 0U;
        const uint32 shadow_uniform_upload_count = std::max(shadow_cascades, 1U);

        out_maps.count = shadow_layer_count;
        out_maps.buffers.clear();
        out_maps.buffers.reserve(shadow_uniform_upload_count);

        for (uint32 shadow_index = 0U; shadow_index < shadow_uniform_upload_count; ++shadow_index)
        {
            auto shadow_pass_data = ShadowShaderData {

            };

            shadow_pass_data.shadow_meta.y = static_cast<int32>(shadow_index);
            const GraphicsResourceBinding shadow_pass_uniform_buffer =
                resource_uploader.upload_uniform_buffer(
                    resource_tracker,
                    BINDING_SHADOW_PASS_DATA,
                    "Shadow Pass Shader Data",
                    std::string("Toybox/Uniforms/ShadowPass/") + std::to_string(shadow_index),
                    frame_index,
                    &shadow_pass_data,
                    static_cast<uint64>(sizeof(shadow_pass_data)));
            if (!shadow_pass_uniform_buffer.resource.is_valid())
            {
                return Result(
                    false,
                    "Frame pipeline factory failed: shadow uniform upload failed.");
            }

            out_maps.buffers.push_back(shadow_pass_uniform_buffer);
        }

        const uint32 shadow_resolution = std::max(shadow_resolution, 1U);
        out_maps.map =
            has_shadowed_light
                ? resource_uploader.upload_texture(
                      resource_tracker,
                      BINDING_SHADOW_MAP,
                      std::string("Toybox/ShadowMap/") + std::to_string(shadow_resolution) + "/"
                          + std::to_string(shadow_layer_count),
                      GraphicsTextureDesc {
                          .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
                          .format = GraphicsTextureFormat::DEPTH32_FLOAT,
                          .size = Size {shadow_resolution, shadow_resolution},
                          .mip_count = 1U,
                          .array_layer_count = shadow_layer_count,
                          .debug_name = "Toybox Shadow Map",
                      })
                : GraphicsResourceBinding {.slot = BINDING_SHADOW_MAP};
        if (has_shadowed_light && !out_maps.map.resource.is_valid())
            return Result(false, "Frame pipeline factory failed: shadow map upload failed.");

        return {};
    }

    static Result create_gbuffer(
        // TODO: Introduce 'ResourceManager' that wraps uploader and tracker. It exposes wrapper
        // methods for ease of use and the tracker and uploader remain the implementations.
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        const FrameData& frame,
        GBuffer& out_buff)
    {
        const Size viewport_size = frame.viewport.dimensions;
        const std::string render_target_key =
            std::to_string(viewport_size.width) + "x" + std::to_string(viewport_size.height);

        out_buff.albedo = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_GBUFFER_ALBEDO,
            "Toybox/GBuffer/Albedo/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Albedo"));
        out_buff.normal = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_GBUFFER_NORMAL,
            "Toybox/GBuffer/Normal/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Normal"));
        out_buff.material = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_GBUFFER_MATERIAL,
            "Toybox/GBuffer/Material/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA8,
                "Toybox GBuffer Material"));
        out_buff.emissive = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_GBUFFER_EMISSIVE,
            "Toybox/GBuffer/Emissive/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox GBuffer Emissive"));
        out_buff.depth = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_GBUFFER_DEPTH,
            "Toybox/GBuffer/Depth/" + render_target_key,
            make_deferred_depth_target_desc(viewport_size, "Toybox GBuffer Depth"));
        out_buff.final_color = resource_uploader.upload_texture(
            resource_tracker,
            BINDING_POST_SOURCE_COLOR,
            "Toybox/FinalColor/" + render_target_key,
            make_deferred_color_target_desc(
                viewport_size,
                GraphicsTextureFormat::RGBA16_FLOAT,
                "Toybox Final Color"));

        if (!out_buff.albedo.resource.is_valid() || !out_buff.normal.resource.is_valid()
            || !out_buff.material.resource.is_valid() || !out_buff.emissive.resource.is_valid()
            || !out_buff.depth.resource.is_valid() || !out_buff.final_color.resource.is_valid())
        {
            return Result(false, "Frame pipeline factory failed: deferred target upload failed.");
        }

        return {};
    }

    static void collect_dynamic_mesh_batches(
        EntityRegistry& entity_registry,
        ResourceUploader& resource_uploader,
        const bool has_shadowed_light,
        const Vec3& camera_position,
        const float shadow_caster_max_distance,
        const MaterialInstance& fallback_material,
        const MaterialInstance& shadow_material,
        const uint64 shadow_material_key,
        MaterialRenderClassificationCache& material_classification_cache,
        RenderBatchCollections& out_batches)
    {
        // Runtime meshes are grouped by mesh pointer and material so matching instances share one
        // GPU draw command with an instance buffer.
#if defined(TBX_ENABLE_VERBOSE)
        auto* metrics = active_render_metrics;
        const auto timer =
            ScopedRenderMetricTimer(metrics ? &metrics->collect_dynamic_mesh_batches_ms : nullptr);
#endif
        for (auto& entity : entity_registry.get_with<DynamicMesh, Transform>())
        {
            const auto& mesh_component = entity.get_component<DynamicMesh>();
            const auto mesh_data = mesh_component.get_data();
            if (!mesh_data)
                continue;

            const auto& mesh = mesh_component.get_mesh();
            if (mesh.vertices.empty() || mesh.indices.empty())
                continue;

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            const Transform transform = get_world_space_transform(entity);
            const auto model_matrix = build_transform_matrix(transform);
            const MaterialInstance& material =
                material_instance ? *material_instance : fallback_material;
            const uint64 source_material_key = hash(material);
            const MaterialRenderClassification& classification =
                resolve_material_render_classification(
                    material,
                    source_material_key,
                    resource_uploader,
                    material_classification_cache);
            const bool is_transparent_material =
                should_material_use_transparent_pass(classification);
            auto forward_material = MaterialInstance();
            const MaterialInstance* render_material = &material;
            if (is_transparent_material)
            {
                forward_material = make_forward_material(material);
                render_material = &forward_material;
            }

            const uint64 material_key =
                is_transparent_material ? hash(*render_material) : source_material_key;
            const auto mesh_source = RenderingMeshSourceType::DYNAMIC_RUNTIME_MESH;
            const Uuid mesh_id =
                Uuid(static_cast<uint32>(reinterpret_cast<std::uintptr_t>(mesh_data.get())));
            const auto instance = RenderingDrawInstanceData {
                .model_matrix = model_matrix,
                .normal_matrix = normal(model_matrix),
            };
            auto& scene_batches = is_transparent_material ? out_batches.transparent_batches
                                                          : out_batches.opaque_batches;
            append_render_batch_instance(
                scene_batches,
                hash_render_batch(mesh_source, mesh_id, mesh_data.get(), material_key),
                mesh_source,
                material_key,
                Handle("Toybox/DynamicMesh"),
                mesh_data,
                *render_material,
                instance);

            if (has_shadowed_light
                && should_material_cast_shadows(
                    classification,
                    transform.position,
                    camera_position,
                    shadow_caster_max_distance))
            {
                append_render_batch_instance(
                    out_batches.shadow_batches,
                    hash_render_batch(mesh_source, mesh_id, mesh_data.get(), shadow_material_key),
                    mesh_source,
                    shadow_material_key,
                    Handle("Toybox/DynamicMeshShadow"),
                    mesh_data,
                    shadow_material,
                    instance);
            }
        }
    }

    static void collect_static_mesh_batches(
        EntityRegistry& entity_registry,
        ResourceUploader& resource_uploader,
        const bool has_shadowed_light,
        const Vec3& camera_position,
        const float shadow_caster_max_distance,
        const MaterialInstance& fallback_material,
        const MaterialInstance& shadow_material,
        const uint64 shadow_material_key,
        MaterialRenderClassificationCache& material_classification_cache,
        RenderBatchCollections& out_batches)
    {
        for (auto& entity : entity_registry.get_with<StaticMesh, Transform>())
        {
            const auto& static_mesh = entity.get_component<StaticMesh>();
            if (!static_mesh.handle.is_valid())
                continue;

            const auto* material_instance = entity.has_component<MaterialInstance>()
                                                ? &entity.get_component<MaterialInstance>()
                                                : nullptr;
            const Transform transform = get_world_space_transform(entity);
            const auto model_matrix = build_transform_matrix(transform);
            const MaterialInstance& material =
                material_instance ? *material_instance : fallback_material;
            const uint64 source_material_key = hash(material);
            const MaterialRenderClassification& classification =
                resolve_material_render_classification(
                    material,
                    source_material_key,
                    resource_uploader,
                    material_classification_cache);
            const bool is_transparent_material =
                should_material_use_transparent_pass(classification);
            auto forward_material = MaterialInstance();
            const MaterialInstance* render_material = &material;
            if (is_transparent_material)
            {
                forward_material = make_forward_material(material);
                render_material = &forward_material;
            }

            const uint64 material_key =
                is_transparent_material ? hash(*render_material) : source_material_key;
            const auto instance = RenderingDrawInstanceData {
                .model_matrix = model_matrix,
                .normal_matrix = normal(model_matrix),
            };
            auto& scene_batches = is_transparent_material ? out_batches.transparent_batches
                                                          : out_batches.opaque_batches;
            append_render_batch_instance(
                scene_batches,
                hash_render_batch(
                    RenderingMeshSourceType::MODEL_ASSET,
                    static_mesh.handle.get_id(),
                    nullptr,
                    material_key),
                RenderingMeshSourceType::MODEL_ASSET,
                material_key,
                static_mesh.handle,
                {},
                *render_material,
                instance);

            if (has_shadowed_light
                && should_material_cast_shadows(
                    classification,
                    transform.position,
                    camera_position,
                    shadow_caster_max_distance))
            {
                append_render_batch_instance(
                    out_batches.shadow_batches,
                    hash_render_batch(
                        RenderingMeshSourceType::MODEL_ASSET,
                        static_mesh.handle.get_id(),
                        nullptr,
                        shadow_material_key),
                    RenderingMeshSourceType::MODEL_ASSET,
                    shadow_material_key,
                    static_mesh.handle,
                    {},
                    shadow_material,
                    instance);
            }
        }
    }

    // Iterate over EVERYTHING at once, just do it in one go
    static RenderBatchCollections collect_scene_batches(
        EntityRegistry& entity_registry,
        ResourceUploader& resource_uploader,
        const ShadowCascades& shadow_resources,
        const Vec3& camera_position,
        const float shadow_caster_max_distance)
    {
        auto batches = RenderBatchCollections();
        const auto fallback_material = MaterialInstance(PbrMaterial::HANDLE);
        // TODO: rename to ShadowMapMaterial
        const auto shadow_material = MaterialInstance(tbx::DirectionalShadowMapMaterial::HANDLE);
        const uint64 shadow_material_key = hash(shadow_material);
        const bool has_shadowed_light = shadow_resources.count > 0U;
        auto material_classification_cache = MaterialRenderClassificationCache();

        collect_dynamic_mesh_batches(
            entity_registry,
            resource_uploader,
            has_shadowed_light,
            camera_position,
            shadow_caster_max_distance,
            fallback_material,
            shadow_material,
            shadow_material_key,
            material_classification_cache,
            batches);
        collect_static_mesh_batches(
            entity_registry,
            resource_uploader,
            has_shadowed_light,
            camera_position,
            shadow_caster_max_distance,
            fallback_material,
            shadow_material,
            shadow_material_key,
            material_classification_cache,
            batches);

        return batches;
    }

    static Result append_render_batch_draws(
        const uint64 frame_index,
        const FrameUniformBindings& frame_uniforms,
        const RenderBatchCollection& batches,
        const std::string& debug_prefix,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        RenderingDrawCommandFactory& draw_command_factory,
        DrawBuildState& draw_state,
        RenderPass& render_pass)
    {
        render_pass.indexed_draws.reserve(render_pass.indexed_draws.size() + batches.size());
        for (const auto& [batch_key, batch] : batches)
        {
            const auto result = draw_command_factory.create(
                frame_index,
                frame_uniforms.buffers,
                RenderingDrawBatchInput {
                    .mesh_source = batch.mesh_source,
                    .mesh_handle = batch.mesh_handle,
                    .batch_key = batch_key,
                    .debug_name = make_batch_debug_name(batch_key, debug_prefix),
                    .dynamic_mesh = batch.dynamic_mesh,
                    .material = batch.material,
                    .material_key = batch.material_key,
                    .instances = batch.instances,
                },
                resource_uploader,
                resource_tracker,
                draw_state.material_uploads,
                draw_state.material_uniform_buffers,
                render_pass.indexed_draws);
            if (!result)
                return result;
        }

        return {};
    }

    static Result build_shadow_pass_draws(
        const uint64 frame_index,
        const FrameUniformBindings& frame_uniforms,
        const ShadowCascades& shadow_resources,
        const RenderBatchCollection& shadow_batches,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        RenderingDrawCommandFactory& draw_command_factory,
        DrawBuildState& draw_state,
        PipelinePasses& passes)
    {
        // Each shadow layer is rendered as its own depth-only pass. The shader uniform identifies
        // which light/cascade layer the pass writes into.
        passes.shadow_passes.clear();
        passes.shadow_passes.reserve(shadow_resources.count);
        for (uint32 shadow_index = 0U; shadow_index < shadow_resources.count; ++shadow_index)
        {
            auto shadow_pass = RenderPass {
                .pass =
                    GraphicsPassDesc {
                        .depth_stencil_target = shadow_resources.map.resource,
                        .depth_stencil_layer = static_cast<int32>(shadow_index),
                        .clear_depth = 1.0F,
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .debug_name = "Toybox Shadow Pass",
                    },
            };
            const auto shadow_uniform_buffers = std::array<GraphicsResourceBinding, 3U> {
                frame_uniforms.buffers[0U],
                frame_uniforms.buffers[1U],
                shadow_resources.buffers[shadow_index],
            };

            shadow_pass.indexed_draws.reserve(shadow_batches.size());
            for (const auto& [batch_key, batch] : shadow_batches)
            {
                const auto result = draw_command_factory.create(
                    frame_index,
                    shadow_uniform_buffers,
                    RenderingDrawBatchInput {
                        .mesh_source = batch.mesh_source,
                        .mesh_handle = batch.mesh_handle,
                        .batch_key = batch_key,
                        .debug_name = make_batch_debug_name(batch_key, "Toybox/ShadowBatch/"),
                        .dynamic_mesh = batch.dynamic_mesh,
                        .material = batch.material,
                        .material_key = batch.material_key,
                        .instances = batch.instances,
                    },
                    resource_uploader,
                    resource_tracker,
                    draw_state.material_uploads,
                    draw_state.material_uniform_buffers,
                    shadow_pass.indexed_draws);
                if (!result)
                    return result;
            }

            passes.shadow_passes.push_back(std::move(shadow_pass));
        }

        return {};
    }

    static Result build_scene_pass_draws(
        const uint64 frame_index,
        const FrameUniformBindings& frame_uniforms,
        const RenderBatchCollections& batches,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        RenderingDrawCommandFactory& draw_command_factory,
        DrawBuildState& draw_state,
        PipelinePasses& passes)
    {
        // Opaque geometry feeds the deferred GBuffer. Transparent geometry is drawn after lighting
        // because blended materials need the already-lit final color underneath them.
        auto result = append_render_batch_draws(
            frame_index,
            frame_uniforms,
            batches.opaque_batches,
            "Toybox/RenderBatch/",
            resource_uploader,
            resource_tracker,
            draw_command_factory,
            draw_state,
            passes.opaque_pass);
        if (!result)
            return result;

        result = append_render_batch_draws(
            frame_index,
            frame_uniforms,
            batches.transparent_batches,
            "Toybox/TransparentBatch/",
            resource_uploader,
            resource_tracker,
            draw_command_factory,
            draw_state,
            passes.transparent_pass);
        if (!result)
            return result;

        return {};
    }

    static Result build_skybox_pass_draws(
        const uint64 frame_index,
        EntityRegistry& entity_registry,
        const FrameUniformBindings& frame_uniforms,
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        RenderingDrawCommandFactory& draw_command_factory,
        DrawBuildState& draw_state,
        std::shared_ptr<Mesh>& sky_mesh,
        RenderPass& skybox_pass)
    {
        // Sky is a forward draw into final_color. It runs before deferred lighting when opaque
        // geometry exists so lighting can preserve sky pixels where the GBuffer has no scene depth.
        for (auto& entity : entity_registry.get_with<Sky>())
        {
            const auto& sky = entity.get_component<Sky>();
            const auto& sky_mesh_handle = get_sky_mesh_handle(sky.type);
            if (!sky_mesh && !resource_uploader.has_static_runtime_mesh(sky_mesh_handle))
                sky_mesh = std::make_shared<Mesh>(get_sky_mesh(sky.type));

            auto sky_transform =
                entity.has_component<Transform>() ? get_world_space_transform(entity) : Transform();
            sky_transform.position = Vec3(0.0F);
            sky_transform.scale = Vec3(1.0F);
            const auto model_matrix = build_transform_matrix(sky_transform);
            const auto material = make_sky_material_instance(sky);
            const uint64 material_key = hash(material);
            const auto result = draw_command_factory.create(
                frame_index,
                frame_uniforms.buffers,
                RenderingDrawBatchInput {
                    .mesh_source = RenderingMeshSourceType::STATIC_RUNTIME_MESH,
                    .mesh_handle = sky_mesh_handle,
                    .batch_key = hash(entity.get_id(), material_key),
                    .debug_name = std::string("Toybox/Sky/Entity/") + to_string(entity.get_id()),
                    .runtime_mesh = sky_mesh,
                    .material = material,
                    .material_key = material_key,
                    .instances =
                        {
                            RenderingDrawInstanceData {
                                .model_matrix = model_matrix,
                                .normal_matrix = normal(model_matrix),
                            },
                        },
                },
                resource_uploader,
                resource_tracker,
                draw_state.material_uploads,
                draw_state.material_uniform_buffers,
                skybox_pass.indexed_draws);
            if (!result)
                return result;

            sky_mesh.reset();
        }

        return {};
    }

    static void append_shadow_sampling_resources(
        const GraphicsResourceBinding& shadow_sampling_uniform_buffer,
        const GraphicsResourceBinding& shadow_map,
        RenderPass& render_pass)
    {
        // Forward transparent materials need the same shadow data that deferred lighting samples.
        for (auto& draw : render_pass.draws)
        {
            draw.uniform_buffers.push_back(shadow_sampling_uniform_buffer);
            if (shadow_map.resource.is_valid())
                draw.textures.push_back(shadow_map);
        }

        for (auto& draw : render_pass.indexed_draws)
        {
            draw.uniform_buffers.push_back(shadow_sampling_uniform_buffer);
            if (shadow_map.resource.is_valid())
                draw.textures.push_back(shadow_map);
        }
    }

    static Result append_fullscreen_draw(
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        const FrameUniformBindings& frame_uniforms,
        const MaterialInstance& material,
        const std::vector<GraphicsResourceBinding>& uniform_buffers,
        const std::vector<GraphicsResourceBinding>& textures,
        RenderPass& render_pass)
    {
        auto material_upload = RenderingMaterialUploadData();
        const Result result =
            resource_uploader.upload_material(material, resource_tracker, material_upload);
        if (!result)
            return result;
        if (!material_upload.pipeline.is_valid())
            return Result(
                false,
                "Frame pipeline factory failed: fullscreen pipeline upload failed.");

        // Fullscreen passes use a generated triangle. Lighting samples the GBuffer; post process
        // samples final_color and writes to the swapchain render target.
        auto draw = GraphicsDrawCommand {
            .pipeline = material_upload.pipeline,
            .uniform_buffers = uniform_buffers.empty()
                                   ? std::vector<GraphicsResourceBinding> {
                                         frame_uniforms.buffers[0U],
                                         frame_uniforms.buffers[1U],
                                         frame_uniforms.buffers[2U],
                                     }
                                   : uniform_buffers,
            .textures = material_upload.textures,
            .vertex_count = 3U,
        };
        draw.textures.insert(draw.textures.end(), textures.begin(), textures.end());
        render_pass.draws.push_back(std::move(draw));
        return {};
    }

    static Result append_pipeline_passes(
        ResourceUploader& resource_uploader,
        RenderingResourceTracker& resource_tracker,
        const FrameUniformBindings& frame_uniforms,
        const ShadowCascades& shadow_resources,
        const GBuffer& gbuffer,
        PipelinePasses& passes,
        std::vector<RenderPass>& out_render_passes)
    {
        // The final pass list is ordered exactly as the GPU needs it: shadow maps, GBuffer,
        // optional sky, deferred lighting, transparent forward, then post process.
        for (auto& shadow_pass : passes.shadow_passes)
            out_render_passes.push_back(std::move(shadow_pass));

        const bool has_gbuffer_pass = has_draw_commands(passes.opaque_pass);
        const bool has_skybox_pass = has_draw_commands(passes.skybox_pass);
        const bool has_transparent_pass = has_draw_commands(passes.transparent_pass);

        if (has_gbuffer_pass)
            out_render_passes.push_back(std::move(passes.opaque_pass));

        if (has_draw_commands(passes.alpha_cutout_pass))
            out_render_passes.push_back(std::move(passes.alpha_cutout_pass));

        const bool should_add_lighting_pass = has_gbuffer_pass;
        if (should_add_lighting_pass)
        {
            if (has_skybox_pass)
            {
                out_render_passes.push_back(std::move(passes.skybox_pass));
            }
            else
            {
                passes.lighting_pass.pass.clear_flags = GraphicsClearFlags::COLOR;
            }

            auto lighting_textures = std::vector<GraphicsResourceBinding> {
                gbuffer.gbuffer_albedo,
                gbuffer.gbuffer_normal,
                gbuffer.gbuffer_material,
                gbuffer.gbuffer_emissive,
                gbuffer.gbuffer_depth,
            };
            if (shadow_resources.map.resource.is_valid())
                lighting_textures.push_back(shadow_resources.map);

            auto lighting_uniforms = std::vector<GraphicsResourceBinding> {
                frame_uniforms.buffers[0U],
                frame_uniforms.buffers[1U],
                frame_uniforms.buffers[2U],
                shadow_resources.uniform_buffers.front(),
            };
            const auto lighting_result = append_fullscreen_draw(
                resource_uploader,
                resource_tracker,
                frame_uniforms,
                MaterialInstance(Handle("Materials/DeferredLighting.mat")),
                lighting_uniforms,
                lighting_textures,
                passes.lighting_pass);
            if (!lighting_result)
                return lighting_result;

            out_render_passes.push_back(std::move(passes.lighting_pass));
        }
        else if (has_skybox_pass)
        {
            out_render_passes.push_back(std::move(passes.skybox_pass));
        }

        if (has_transparent_pass)
        {
            if (!should_add_lighting_pass && !has_skybox_pass)
                passes.transparent_pass.pass.clear_flags = GraphicsClearFlags::COLOR;

            append_shadow_sampling_resources(
                shadow_resources.buffers.front(),
                shadow_resources.map,
                passes.transparent_pass);
            out_render_passes.push_back(std::move(passes.transparent_pass));
        }

        if (should_add_lighting_pass || has_skybox_pass || has_transparent_pass)
        {
            const auto post_result = append_fullscreen_draw(
                resource_uploader,
                resource_tracker,
                frame_uniforms,
                MaterialInstance(Handle("Materials/TonemapPost.mat")),
                {},
                {gbuffer.final_color},
                passes.post_process_pass);
            if (!post_result)
                return post_result;

            out_render_passes.push_back(std::move(passes.post_process_pass));
        }

        return {};
    }
}
