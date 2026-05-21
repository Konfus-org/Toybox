#pragma once
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/rendering_pass_factory.h"
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

    struct RenderBatch
    {
        uint64 batch_key = 0U;
        RenderingMeshSourceType mesh_source = RenderingMeshSourceType::MODEL_ASSET;
        uint64 material_key = 0U;
        Handle mesh_handle = {};
        std::shared_ptr<DynamicMeshData> dynamic_mesh = {};
        MaterialInstance material = {};
        std::vector<RenderingDrawInstanceData> instances = {};
    };

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

    static bool has_shadowed_local_light(
        EntityRegistry& entity_registry,
        const Vec3& camera_position,
        const float local_light_max_distance)
    {
        for (auto& entity : entity_registry.get_with<PointLight, Transform>())
        {
            const auto& light = entity.get_component<PointLight>();
            const Transform transform = get_world_space_transform(entity);
            if (light.cast_shadows
                && is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                return true;
            }
        }

        for (auto& entity : entity_registry.get_with<SpotLight, Transform>())
        {
            const auto& light = entity.get_component<SpotLight>();
            const Transform transform = get_world_space_transform(entity);
            if (light.cast_shadows
                && is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                return true;
            }
        }

        for (auto& entity : entity_registry.get_with<AreaLight, Transform>())
        {
            const auto& light = entity.get_component<AreaLight>();
            const Transform transform = get_world_space_transform(entity);
            if (light.cast_shadows
                && is_within_distance_limit(
                    transform.position,
                    camera_position,
                    local_light_max_distance))
            {
                return true;
            }
        }

        return false;
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
        auto ambient_color = Vec3(0.0F);
        auto has_directional_ambient = false;
        auto has_shadowed_light = false;
        auto directional_lights = std::vector<ShaderLightData> {};
        auto directional_shadow_flags = std::vector<bool> {};

        for (auto& entity : entity_registry.get_with<DirectionalLight, Transform>())
        {
            const auto& light = entity.get_component<DirectionalLight>();
            const Transform transform = get_world_space_transform(entity);
            const Vec3 direction = normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
            const Vec3 color = make_light_color(light);

            ambient_color += color * light.ambient;
            has_directional_ambient = true;

            directional_lights.push_back(
                ShaderLightData {
                    .position_type = Vec4(0.0F, 0.0F, 0.0F, SHADER_LIGHT_TYPE_DIRECTIONAL),
                    .direction_range = Vec4(direction, 0.0F),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(0.0F, 0.0F, -1.0F, 0.0F),
                });
            directional_shadow_flags.push_back(light.cast_shadows);
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

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_POINT),
                    .direction_range = Vec4(0.0F, 0.0F, 0.0F, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        0.0F,
                        0.0F,
                        light.cast_shadows && !has_shadowed_light ? 0.0F : -1.0F,
                        0.0F),
                });
            if (light.cast_shadows && !has_shadowed_light)
            {
                has_shadowed_light = true;
                light_data.light_meta.y = 1;
            }
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

            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_SPOT),
                    .direction_range = Vec4(direction, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        angle_to_cosine(light.inner_angle),
                        angle_to_cosine(light.outer_angle),
                        light.cast_shadows && !has_shadowed_light ? 0.0F : -1.0F,
                        0.0F),
                });
            if (light.cast_shadows && !has_shadowed_light)
            {
                has_shadowed_light = true;
                light_data.light_meta.y = 1;
            }
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
            append_shader_light(
                light_data,
                ShaderLightData {
                    .position_type = Vec4(transform.position, SHADER_LIGHT_TYPE_POINT),
                    .direction_range = Vec4(0.0F, 0.0F, 0.0F, light.range),
                    .color_intensity = Vec4(color, light.intensity),
                    .params = Vec4(
                        0.0F,
                        0.0F,
                        light.cast_shadows && !has_shadowed_light ? 0.0F : -1.0F,
                        0.0F),
                });
            if (light.cast_shadows && !has_shadowed_light)
            {
                has_shadowed_light = true;
                light_data.light_meta.y = 1;
            }
        }

        for (size index = 0U; index < directional_lights.size(); ++index)
        {
            auto shader_light = directional_lights[index];
            if (directional_shadow_flags[index] && !has_shadowed_light)
            {
                shader_light.params.z = 0.0F;
                has_shadowed_light = true;
                light_data.light_meta.y = 1;
            }

            append_shader_light(light_data, shader_light);
        }

        if (has_directional_ambient)
            light_data.ambient_color = Vec4(ambient_color, 1.0F);

        return light_data;
    }

    static ShadowPassShaderData build_shadow_shader_data(
        EntityRegistry& entity_registry,
        const Vec3& camera_position,
        const float shadow_render_distance,
        const float shadow_softness,
        const float local_light_max_distance)
    {
        auto shadow_data = ShadowPassShaderData();
        const float shadow_distance = std::max(shadow_render_distance, 1.0F);
        const bool local_shadow_owner =
            has_shadowed_local_light(entity_registry, camera_position, local_light_max_distance);
        if (!local_shadow_owner)
        {
            for (auto& entity : entity_registry.get_with<DirectionalLight, Transform>())
            {
                const auto& light = entity.get_component<DirectionalLight>();
                if (!light.cast_shadows)
                    continue;

                const Transform transform = get_world_space_transform(entity);
                const Vec3 direction =
                    normalize_or_zero(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
                const Vec3 light_position = camera_position - direction * (shadow_distance * 0.5F);
                const Mat4 view =
                    look_at(light_position, camera_position, make_shadow_up_vector(direction));
                const float half_extent = shadow_distance * 0.5F;
                const Mat4 projection = ortho_projection(
                    -half_extent,
                    half_extent,
                    -half_extent,
                    half_extent,
                    0.1F,
                    shadow_distance);

                shadow_data.light_view_projection = projection * view;
                shadow_data.light_direction = Vec4(direction, 0.0F);
                shadow_data.shadow_depth_bias = 0.0015F;
                shadow_data.shadow_normal_bias = 0.02F;
                shadow_data.shadow_strength = 0.75F;
                (void)shadow_softness;
                return shadow_data;
            }
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

            shadow_data.light_view_projection = projection * view;
            shadow_data.light_direction = Vec4(direction, 0.0F);
            shadow_data.shadow_depth_bias = 0.0015F;
            shadow_data.shadow_normal_bias = 0.02F;
            shadow_data.shadow_strength = 0.85F;
            (void)shadow_softness;
            return shadow_data;
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

            shadow_data.light_view_projection = projection * view;
            shadow_data.light_direction = Vec4(direction, 0.0F);
            shadow_data.shadow_depth_bias = 0.0015F;
            shadow_data.shadow_normal_bias = 0.02F;
            shadow_data.shadow_strength = 0.85F;
            (void)shadow_softness;
            return shadow_data;
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

            shadow_data.light_view_projection = projection * view;
            shadow_data.light_direction = Vec4(direction, 0.0F);
            shadow_data.shadow_depth_bias = 0.0015F;
            shadow_data.shadow_normal_bias = 0.02F;
            shadow_data.shadow_strength = 0.85F;
            (void)shadow_softness;
            return shadow_data;
        }

        return shadow_data;
    }

    static bool should_material_cast_shadows(
        const MaterialInstance& material,
        const ResourceUploader& resource_uploader,
        const Vec3& position,
        const Vec3& camera_position,
        const float shadow_caster_max_distance)
    {
        const ShadowMode shadow_mode = resource_uploader.get_material_config(material).shadow_mode;
        if (shadow_mode == ShadowMode::NONE)
            return false;
        if (shadow_mode == ShadowMode::ALWAYS)
            return true;

        return is_within_distance_limit(position, camera_position, shadow_caster_max_distance);
    }

    static bool should_material_use_transparent_pass(
        const MaterialInstance& material,
        const ResourceUploader& resource_uploader)
    {
        return resource_uploader.get_material_config(material).blend_mode
               == MaterialBlendMode::ALPHA_BLEND;
    }

    using RenderBatchCollection = std::unordered_map<uint64, RenderBatch>;

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
}
