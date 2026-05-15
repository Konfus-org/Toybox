#include "tbx/systems/graphics/pipeline/commands/directional_shadow_pass_operation.h"
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
    struct ShadowBatch
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        Uuid instance_buffer = {};
        uint32 index_count = 0U;
        std::vector<Mat4> transforms = {};
    };

    struct ShadowCasterDraw
    {
        uint64 base_batch_key = 0U;
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        uint32 index_count = 0U;
        Mat4 model_to_world = Mat4(1.0F);
        Sphere world_bounds = {};
    };

    struct PointShadowPassUniform
    {
        Mat4 world_to_light = Mat4(1.0F);
        Vec4 shadow_params = Vec4(1.0F, 1.0F, 0.0F, 0.0F);
    };

    static uint64 make_shadow_pass_batch_key(const uint64 base_batch_key, const uint64 pass_key)
    {
        return fnv1a_hash_value(pass_key, fnv1a_hash_value(base_batch_key, TBX_FNV1A_OFFSET_BASIS));
    }

    static bool shadow_caster_intersects_sphere(
        const Sphere& caster_bounds,
        const Vec3& center,
        const float radius)
    {
        const Vec3 delta = caster_bounds.center - center;
        const float distance_squared = glm::dot(delta, delta);
        const float radius_sum = caster_bounds.radius + radius;
        return distance_squared <= (radius_sum * radius_sum);
    }

    static bool shadow_caster_within_max_camera_distance(
        const Vec3& camera_position,
        const Sphere& world_bounds,
        const float max_distance)
    {
        if (max_distance <= 0.0F)
            return true;
        const Vec3 delta = world_bounds.center - camera_position;
        const float center_distance = glm::length(delta);
        const float closest_surface = center_distance - std::max(world_bounds.radius, 0.0F);
        return closest_surface <= max_distance;
    }

    static Shader make_directional_shadow_shader()
    {
        return Shader(
            std::vector<ShaderSource> {
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) in vec3 a_position;\n"
                    "layout(location = 5) in vec4 a_model0;\n"
                    "layout(location = 6) in vec4 a_model1;\n"
                    "layout(location = 7) in vec4 a_model2;\n"
                    "layout(location = 8) in vec4 a_model3;\n"
                    "layout(std140, binding = 0) uniform ToyboxGlobalBlock\n"
                    "{\n"
                    "    mat4 u_view_proj;\n"
                    "};\n"
                    "void main()\n"
                    "{\n"
                    "    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);\n"
                    "    gl_Position = u_view_proj * model * vec4(a_position, 1.0);\n"
                    "}\n",
                    ShaderType::VERTEX),
                ShaderSource(
                    "#version 450 core\n"
                    "void main()\n"
                    "{\n"
                    "}\n",
                    ShaderType::FRAGMENT),
            });
    }

    static GraphicsPipelineDesc make_directional_shadow_pipeline_desc()
    {
        constexpr uint32 model_stride = static_cast<uint32>(sizeof(float) * 16U);
        return GraphicsPipelineDesc {
            .shader = make_directional_shadow_shader(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = model_stride,
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 1U,
                        .stride = static_cast<uint32>(sizeof(Mat4)),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes =
                {
                    GraphicsVertexAttributeDesc {
                        .location = 0U,
                        .buffer_slot = 0U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 5U,
                        .buffer_slot = 1U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 6U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 4U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 7U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 8U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 8U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 12U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                },
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_blending_enabled = false,
            .is_culling_enabled = true,
            .cull_mode = GraphicsCullMode::FRONT,
            .debug_name = "Toybox Directional Shadow Pipeline",
        };
    }

    static Shader make_point_shadow_shader()
    {
        return Shader(
            std::vector<ShaderSource> {
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) in vec3 a_position;\n"
                    "layout(location = 5) in vec4 a_model0;\n"
                    "layout(location = 6) in vec4 a_model1;\n"
                    "layout(location = 7) in vec4 a_model2;\n"
                    "layout(location = 8) in vec4 a_model3;\n"
                    "layout(std140, binding = 1) uniform ToyboxPassBlock\n"
                    "{\n"
                    "    mat4 u_world_to_light;\n"
                    "    vec4 u_shadow_params;\n"
                    "};\n"
                    "out vec2 v_depth_and_facing;\n"
                    "void main()\n"
                    "{\n"
                    "    mat4 model = mat4(a_model0, a_model1, a_model2, a_model3);\n"
                    "    vec3 world_position = (model * vec4(a_position, 1.0)).xyz;\n"
                    "    vec3 light_space = (u_world_to_light * vec4(world_position, 1.0)).xyz;\n"
                    "    float distance_to_light = length(light_space);\n"
                    "    vec3 direction = light_space / max(distance_to_light, 0.0001);\n"
                    "    float hemisphere_sign = u_shadow_params.y;\n"
                    "    float facing = hemisphere_sign > 0.0 ? direction.z : -direction.z;\n"
                    "    float denominator = hemisphere_sign > 0.0\n"
                    "                            ? max(1.0 + direction.z, 0.0001)\n"
                    "                            : max(1.0 - direction.z, 0.0001);\n"
                    "    vec2 projected = direction.xy / denominator;\n"
                    "    gl_Position = vec4(projected, 0.0, 1.0);\n"
                    "    v_depth_and_facing = vec2(\n"
                    "        distance_to_light / max(u_shadow_params.x, 0.0001),\n"
                    "        facing);\n"
                    "}\n",
                    ShaderType::VERTEX),
                ShaderSource(
                    "#version 450 core\n"
                    "in vec2 v_depth_and_facing;\n"
                    "void main()\n"
                    "{\n"
                    "    if (v_depth_and_facing.y <= 0.0)\n"
                    "        discard;\n"
                    "    gl_FragDepth = clamp(v_depth_and_facing.x, 0.0, 1.0);\n"
                    "}\n",
                    ShaderType::FRAGMENT),
            });
    }

    static GraphicsPipelineDesc make_point_shadow_pipeline_desc()
    {
        constexpr uint32 model_stride = static_cast<uint32>(sizeof(float) * 16U);
        return GraphicsPipelineDesc {
            .shader = make_point_shadow_shader(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = model_stride,
                    },
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 1U,
                        .stride = static_cast<uint32>(sizeof(Mat4)),
                        .is_per_instance = true,
                    },
                },
            .vertex_attributes =
                {
                    GraphicsVertexAttributeDesc {
                        .location = 0U,
                        .buffer_slot = 0U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC3,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 5U,
                        .buffer_slot = 1U,
                        .offset = 0U,
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 6U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 4U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 7U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 8U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                    GraphicsVertexAttributeDesc {
                        .location = 8U,
                        .buffer_slot = 1U,
                        .offset = static_cast<uint32>(sizeof(float) * 12U),
                        .format = GraphicsVertexFormat::VEC4,
                    },
                },
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_blending_enabled = false,
            .is_culling_enabled = true,
            .cull_mode = GraphicsCullMode::FRONT,
            .debug_name = "Toybox Point Shadow Pipeline",
        };
    }

    static uint32 clamp_shadow_resolution(const uint32 requested_resolution)
    {
        return std::clamp(requested_resolution, 512U, 4096U);
    }

    static bool material_casts_standard_shadow(const MaterialConfig& config)
    {
        if (config.shadow_mode == ShadowMode::None)
            return false;
        if (config.shadow_mode == ShadowMode::Always)
            return true;
        return config.blend_mode == MaterialBlendMode::Opaque;
    }

    static std::vector<Vec3> make_cascade_corners(
        const RenderData& frame_data,
        const float split_near,
        const float split_far)
    {
        const Vec3 forward =
            normalize(frame_data.camera_transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
        const Vec3 right = normalize(frame_data.camera_transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
        const Vec3 up = normalize(frame_data.camera_transform.rotation * Vec3(0.0F, 1.0F, 0.0F));

        const float fov_radians = frame_data.camera.get_fov() * 0.017453292519943295F;
        const float near_height = 2.0F * std::tan(fov_radians * 0.5F) * split_near;
        const float near_width = near_height * frame_data.camera.get_aspect();
        const float far_height = 2.0F * std::tan(fov_radians * 0.5F) * split_far;
        const float far_width = far_height * frame_data.camera.get_aspect();
        const Vec3 near_center = frame_data.camera_position + (forward * split_near);
        const Vec3 far_center = frame_data.camera_position + (forward * split_far);

        return std::vector<Vec3> {
            near_center - (right * (near_width * 0.5F)) - (up * (near_height * 0.5F)),
            near_center + (right * (near_width * 0.5F)) - (up * (near_height * 0.5F)),
            near_center - (right * (near_width * 0.5F)) + (up * (near_height * 0.5F)),
            near_center + (right * (near_width * 0.5F)) + (up * (near_height * 0.5F)),
            far_center - (right * (far_width * 0.5F)) - (up * (far_height * 0.5F)),
            far_center + (right * (far_width * 0.5F)) - (up * (far_height * 0.5F)),
            far_center - (right * (far_width * 0.5F)) + (up * (far_height * 0.5F)),
            far_center + (right * (far_width * 0.5F)) + (up * (far_height * 0.5F)),
        };
    }

    static RenderDataDirectionalShadowCascade make_shadow_cascade(
        const RenderData& frame_data,
        const RenderDataDirectionalLight& light,
        const float split_near,
        const float split_far,
        const float previous_split_far,
        const uint32 shadow_resolution,
        const Uuid texture)
    {
        const auto corners = make_cascade_corners(frame_data, split_near, split_far);
        auto center = Vec3(0.0F);
        for (const Vec3& corner : corners)
            center += corner;
        center /= static_cast<float>(corners.size());

        const Vec3 light_direction = make_light_direction(light.transform);
        Vec3 light_up = Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(dot(light_direction, light_up)) > 0.95F)
            light_up = Vec3(1.0F, 0.0F, 0.0F);

        float radius = 0.0F;
        for (const Vec3& corner : corners)
            radius = std::max(radius, distance(center, corner));
        radius = std::ceil(radius * 16.0F) / 16.0F;

        const float texel_size = (radius * 2.0F) / static_cast<float>(shadow_resolution);
        if (texel_size > 0.0001F)
        {
            const Mat4 snap_view = look_at(center - (light_direction * radius), center, light_up);
            const Vec4 light_center = snap_view * Vec4(center, 1.0F);
            const float snapped_x = std::floor(light_center.x / texel_size) * texel_size;
            const float snapped_y = std::floor(light_center.y / texel_size) * texel_size;
            const Vec4 snapped_light_center = Vec4(snapped_x, snapped_y, light_center.z, 1.0F);
            center = Vec3(inverse(snap_view) * snapped_light_center);
        }

        const Mat4 light_view =
            look_at(center - (light_direction * (radius * 2.0F)), center, light_up);
        float minimum_z = std::numeric_limits<float>::max();
        float maximum_z = std::numeric_limits<float>::lowest();
        for (const Vec3& corner : corners)
        {
            const Vec4 light_space_corner = light_view * Vec4(corner, 1.0F);
            minimum_z = std::min(minimum_z, light_space_corner.z);
            maximum_z = std::max(maximum_z, light_space_corner.z);
        }

        const float depth_margin = std::max(6.0F, radius * 0.5F);
        const float near_plane = std::max(0.1F, -maximum_z - depth_margin);
        const float far_plane = std::max(near_plane + 1.0F, -minimum_z + depth_margin);
        const Mat4 light_projection =
            ortho_projection(-radius, radius, -radius, radius, near_plane, far_plane);

        const float cascade_span = std::max(split_far - previous_split_far, 0.001F);
        const float normal_bias = std::clamp(texel_size * 0.06F, 0.0002F, 0.006F);
        const float depth_bias = std::clamp(texel_size * 0.0025F, 0.00002F, 0.0006F);
        return RenderDataDirectionalShadowCascade {
            .light_view_projection = light_projection * light_view,
            .split_depth = split_far,
            .normal_bias = normal_bias,
            .depth_bias = depth_bias,
            .blend_distance = cascade_span * 0.12F,
            .texture = texture,
        };
    }

    static RenderDataProjectedShadowMap make_spot_shadow_map(
        const RenderDataSpotLight& light,
        const uint32 texture_layer,
        const uint32 shadow_resolution)
    {
        const Vec3 position = light.transform.position;
        const Vec3 direction = make_light_direction(light.transform);
        Vec3 up = Vec3(0.0F, 1.0F, 0.0F);
        if (std::abs(dot(direction, up)) > 0.95F)
            up = Vec3(1.0F, 0.0F, 0.0F);

        const float z_near = 0.05F;
        const float z_far = std::max(light.light.range, z_near + 0.1F);
        const float fov = std::clamp(light.light.outer_angle * 2.0F, 5.0F, 170.0F);
        const float fov_radians = fov * 0.017453292519943295F;
        const Mat4 light_view = look_at(position, position + direction, up);
        const Mat4 light_projection = perspective_projection(fov_radians, 1.0F, z_near, z_far);
        const float texel_world = z_far / static_cast<float>(std::max(shadow_resolution, 1U));

        return RenderDataProjectedShadowMap {
            .entity_uuid = light.entity_uuid,
            .light_view_projection = light_projection * light_view,
            .z_near = z_near,
            .z_far = z_far,
            .normal_bias = std::clamp(texel_world * 0.35F, 0.0002F, 0.006F),
            .depth_bias = std::clamp(texel_world * 0.003F, 0.00003F, 0.001F),
            .texture_layer = texture_layer,
        };
    }

    static RenderDataProjectedShadowMap make_area_shadow_map(
        const RenderDataAreaLight& light,
        const uint32 texture_layer,
        const uint32 shadow_resolution)
    {
        const Vec3 position = light.transform.position;
        const Vec3 direction = make_light_direction(light.transform);
        Vec3 up = normalize(light.transform.rotation * Vec3(0.0F, 1.0F, 0.0F));
        if (std::abs(dot(direction, up)) > 0.95F)
            up = Vec3(1.0F, 0.0F, 0.0F);

        const float z_near = 0.1F;
        const float z_far = std::max(light.light.range, z_near + 0.1F);
        const float extent = std::max(light.light.area_size.x, light.light.area_size.y);
        const float fov_radians =
            std::clamp(2.0F * std::atan((extent + 0.25F) / z_near), 0.2617994F, 2.9670596F);
        const Mat4 light_view = look_at(position, position + direction, up);
        const Mat4 light_projection = perspective_projection(fov_radians, 1.0F, z_near, z_far);
        const float texel_world = z_far / static_cast<float>(std::max(shadow_resolution, 1U));

        return RenderDataProjectedShadowMap {
            .entity_uuid = light.entity_uuid,
            .light_view_projection = light_projection * light_view,
            .z_near = z_near,
            .z_far = z_far,
            .normal_bias = std::clamp(texel_world * 0.35F, 0.0002F, 0.006F),
            .depth_bias = std::clamp(texel_world * 0.003F, 0.00003F, 0.001F),
            .texture_layer = texture_layer,
        };
    }

    static RenderDataPointShadowMap make_point_shadow_map(
        const RenderDataPointLight& light,
        const uint32 texture_layer_offset,
        const uint32 shadow_resolution)
    {
        auto light_transform = light.transform;
        light_transform.scale = Vec3(1.0F);
        const float texel_world =
            std::max(light.light.range, 0.1F) / static_cast<float>(std::max(shadow_resolution, 1U));
        return RenderDataPointShadowMap {
            .entity_uuid = light.entity_uuid,
            .world_to_light = inverse(build_transform_matrix(light_transform)),
            .range = std::max(light.light.range, 0.1F),
            .normal_bias = std::clamp(texel_world * 0.5F, 0.0003F, 0.008F),
            .depth_bias = std::clamp(texel_world * 0.04F, 0.00005F, 0.0015F),
            .layer_offset = texture_layer_offset,
        };
    }

    static std::vector<float> make_cascade_splits(const RenderData& frame_data)
    {
        const float near_plane = std::max(frame_data.camera.get_z_near(), 0.05F);
        const float far_plane = std::max(
            near_plane + 1.0F,
            std::min(frame_data.camera.get_z_far(), frame_data.shadow_render_distance));
        constexpr float split_lambda = 0.65F;

        auto splits = std::vector<float> {};
        splits.reserve(TBX_SHADOW_CASCADE_COUNT);
        for (uint32 cascade = 1U; cascade <= TBX_SHADOW_CASCADE_COUNT; ++cascade)
        {
            const float ratio =
                static_cast<float>(cascade) / static_cast<float>(TBX_SHADOW_CASCADE_COUNT);
            const float logarithmic = near_plane * std::pow(far_plane / near_plane, ratio);
            const float uniform = near_plane + ((far_plane - near_plane) * ratio);
            splits.push_back((logarithmic * split_lambda) + (uniform * (1.0F - split_lambda)));
        }
        return splits;
    }

    DirectionalShadowPassOperation::DirectionalShadowPassOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo DirectionalShadowPassOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Directional Shadow Pass Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    Result DirectionalShadowPassOperation::ensure_shadow_pipeline(
        IGraphicsBackend& backend)
    {
        if (_shadow_pipeline.is_valid())
        {
            _resource_manager.get().update(_shadow_pipeline);
            return {};
        }
        return _resource_manager.get().upload(
            make_directional_shadow_pipeline_desc(),
            _shadow_pipeline);
    }

    Result DirectionalShadowPassOperation::ensure_point_shadow_pipeline(
        IGraphicsBackend& backend)
    {
        if (_point_shadow_pipeline.is_valid())
        {
            _resource_manager.get().update(_point_shadow_pipeline);
            return {};
        }
        return _resource_manager.get().upload(
            make_point_shadow_pipeline_desc(),
            _point_shadow_pipeline);
    }

    Result DirectionalShadowPassOperation::ensure_shadow_resources(
        IGraphicsBackend& backend,
        const RenderData& frame_data,
        const uint32 directional_shadow_count,
        const uint32 point_shadow_count,
        const uint32 spot_shadow_count,
        const uint32 area_shadow_count)
    {
        const uint32 resolution = clamp_shadow_resolution(frame_data.shadow_map_resolution);
        const bool resolution_changed = _shadow_resolution != resolution;
        _shadow_resolution = resolution;

        if (directional_shadow_count == 0U)
        {
            if (_directional_shadow_texture.is_valid())
                _resource_manager.get().unload(_directional_shadow_texture);
            _directional_shadow_texture = {};
        }
        else if (resolution_changed || !_directional_shadow_texture.is_valid())
        {
            if (_directional_shadow_texture.is_valid())
                _resource_manager.get().unload(_directional_shadow_texture);
            _directional_shadow_texture = {};

            if (const auto result = _resource_manager.get().upload(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
                        .format = GraphicsTextureFormat::DEPTH32_FLOAT,
                        .size = Size {resolution, resolution},
                        .mip_count = 1U,
                        .array_layer_count = TBX_SHADOW_CASCADE_COUNT,
                        .debug_name = "Toybox Directional Shadow Cascade Array",
                    },
                    nullptr,
                    0U,
                    _directional_shadow_texture);
                !result)
            {
                return result;
            }
        }

        if (resolution_changed)
        {
            if (_point_shadow_texture.is_valid())
                _resource_manager.get().unload(_point_shadow_texture);
            if (_spot_shadow_texture.is_valid())
                _resource_manager.get().unload(_spot_shadow_texture);
            if (_area_shadow_texture.is_valid())
                _resource_manager.get().unload(_area_shadow_texture);
            _point_shadow_texture = {};
            _spot_shadow_texture = {};
            _area_shadow_texture = {};
            _point_shadow_texture_layers = 0U;
            _spot_shadow_texture_layers = 0U;
            _area_shadow_texture_layers = 0U;
        }

        auto ensure_array_shadow_texture = [&](const uint32 required_layers,
                                               Uuid& texture,
                                               uint32& capacity_layers,
                                               const std::string& name) -> Result
        {
            if (required_layers == 0U)
            {
                if (texture.is_valid())
                    _resource_manager.get().unload(texture);
                texture = {};
                capacity_layers = 0U;
                return {};
            }

            if (texture.is_valid() && capacity_layers >= required_layers)
                return {};

            if (texture.is_valid())
            {
                _resource_manager.get().unload(texture);
                texture = {};
                capacity_layers = 0U;
            }

            auto created = Uuid {};
            if (const auto result = _resource_manager.get().upload(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
                        .format = GraphicsTextureFormat::DEPTH32_FLOAT,
                        .size = Size {resolution, resolution},
                        .mip_count = 1U,
                        .array_layer_count = required_layers,
                        .debug_name = name,
                    },
                    nullptr,
                    0U,
                    created);
                !result)
            {
                return result;
            }

            texture = created;
            capacity_layers = required_layers;
            return {};
        };

        if (const auto result = ensure_array_shadow_texture(
                point_shadow_count * 2U,
                _point_shadow_texture,
                _point_shadow_texture_layers,
                "Toybox Point Shadow Maps");
            !result)
        {
            return result;
        }

        if (const auto result = ensure_array_shadow_texture(
                spot_shadow_count,
                _spot_shadow_texture,
                _spot_shadow_texture_layers,
                "Toybox Spot Shadow Maps");
            !result)
        {
            return result;
        }

        if (const auto result = ensure_array_shadow_texture(
                area_shadow_count,
                _area_shadow_texture,
                _area_shadow_texture_layers,
                "Toybox Area Shadow Maps");
            !result)
        {
            return result;
        }

        _resource_manager.get().update(_shadow_pipeline);
        _resource_manager.get().update(_point_shadow_pipeline);
        if (_directional_shadow_texture.is_valid())
            _resource_manager.get().update(_directional_shadow_texture);
        if (_point_shadow_texture.is_valid())
            _resource_manager.get().update(_point_shadow_texture);
        if (_spot_shadow_texture.is_valid())
            _resource_manager.get().update(_spot_shadow_texture);
        if (_area_shadow_texture.is_valid())
            _resource_manager.get().update(_area_shadow_texture);
        return {};
    }

    Result DirectionalShadowPassOperation::ensure_dynamic_mesh_buffers(
        IGraphicsBackend& backend,
        const std::shared_ptr<Mesh>& mesh,
        Uuid& out_vertex_buffer,
        Uuid& out_index_buffer,
        uint32& out_index_count)
    {
        out_vertex_buffer = {};
        out_index_buffer = {};
        out_index_count = 0U;
        if (!mesh || !can_render_mesh_directly(*mesh))
            return {};

        const uint64 mesh_key = make_mesh_cache_key(mesh);
        const auto vertex_it = _mesh_vertex_buffers.find(mesh_key);
        const auto index_it = _mesh_index_buffers.find(mesh_key);
        const auto count_it = _mesh_index_counts.find(mesh_key);
        if (vertex_it != _mesh_vertex_buffers.end() && index_it != _mesh_index_buffers.end()
            && count_it != _mesh_index_counts.end())
        {
            out_vertex_buffer = vertex_it->second;
            out_index_buffer = index_it->second;
            out_index_count = count_it->second;
            _resource_manager.get().update(out_vertex_buffer);
            _resource_manager.get().update(out_index_buffer);
            return {};
        }

        const uint64 vertex_size =
            static_cast<uint64>(mesh->vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh->indices.size()) * static_cast<uint64>(sizeof(uint32));

        auto vertex_buffer = Uuid {};
        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Shadow Dynamic Mesh Vertices",
                },
                mesh->vertices.data(),
                vertex_size,
                vertex_buffer);
            !result)
            return result;

        auto index_buffer = Uuid {};
        if (const auto result = _resource_manager.get().upload(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Shadow Dynamic Mesh Indices",
                },
                mesh->indices.data(),
                index_size,
                index_buffer);
            !result)
        {
            _resource_manager.get().unload(vertex_buffer);
            return result;
        }

        _mesh_vertex_buffers[mesh_key] = vertex_buffer;
        _mesh_index_buffers[mesh_key] = index_buffer;
        _mesh_index_counts[mesh_key] = static_cast<uint32>(mesh->indices.size());
        out_vertex_buffer = vertex_buffer;
        out_index_buffer = index_buffer;
        out_index_count = static_cast<uint32>(mesh->indices.size());
        return {};
    }

    Result DirectionalShadowPassOperation::ensure_instance_buffer(
        IGraphicsBackend& backend,
        const uint64 batch_key,
        const std::vector<Mat4>& transforms,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (transforms.empty())
            return {};

        const uint64 data_size =
            static_cast<uint64>(transforms.size()) * static_cast<uint64>(sizeof(Mat4));
        const auto buffer_it = _instance_buffers.find(batch_key);
        const auto size_it = _instance_buffer_sizes.find(batch_key);
        if (buffer_it == _instance_buffers.end() || size_it == _instance_buffer_sizes.end()
            || data_size > size_it->second)
        {
            if (buffer_it != _instance_buffers.end())
                _resource_manager.get().unload(buffer_it->second);

            auto buffer = Uuid {};
            if (const auto result = _resource_manager.get().upload(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::VERTEX,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Shadow Instance Transforms",
                    },
                    transforms.data(),
                    data_size,
                    buffer);
                !result)
                return result;

            _instance_buffers[batch_key] = buffer;
            _instance_buffer_sizes[batch_key] = data_size;
            out_buffer = buffer;
            return {};
        }

        if (const auto result =
                _resource_manager.get().update(buffer_it->second, transforms.data(), data_size, 0U);
            !result)
            return result;

        out_buffer = buffer_it->second;
        return {};
    }

    Result DirectionalShadowPassOperation::prepare(RenderData& render_data)
    {
        render_data.directional_shadow_cascades.clear();
        render_data.point_shadow_maps.clear();
        render_data.spot_shadow_maps.clear();
        render_data.area_shadow_maps.clear();
        render_data.directional_shadow_passes.clear();
        render_data.point_shadow_passes.clear();
        render_data.spot_shadow_passes.clear();
        render_data.area_shadow_passes.clear();
        render_data.directional_shadow_texture = {};
        render_data.point_shadow_texture = {};
        render_data.spot_shadow_texture = {};
        render_data.area_shadow_texture = {};
        render_data.directional_shadow_light_entity = {};
        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
            return Result(
                false,
                "DirectionalShadowPassOperation requires IGraphicsBackend service.");
        auto& backend = *backend_ptr;

        const RenderDataDirectionalLight* directional_shadow_light = nullptr;
        for (const auto& light : render_data.directional_lights)
        {
            if (!light.light.cast_shadows)
                continue;

            directional_shadow_light = &light;
            break;
        }

        auto candidate_point_shadow_lights = std::vector<const RenderDataPointLight*> {};
        candidate_point_shadow_lights.reserve(TBX_MAX_POINT_SHADOWS);
        for (const auto& light : render_data.point_lights)
        {
            if (!light.light.cast_shadows)
                continue;
            candidate_point_shadow_lights.push_back(&light);
            if (candidate_point_shadow_lights.size() >= TBX_MAX_POINT_SHADOWS)
                break;
        }

        auto candidate_spot_shadow_lights = std::vector<const RenderDataSpotLight*> {};
        candidate_spot_shadow_lights.reserve(TBX_MAX_SPOT_SHADOWS);
        for (const auto& light : render_data.spot_lights)
        {
            if (!light.light.cast_shadows)
                continue;
            candidate_spot_shadow_lights.push_back(&light);
            if (candidate_spot_shadow_lights.size() >= TBX_MAX_SPOT_SHADOWS)
                break;
        }

        auto candidate_area_shadow_lights = std::vector<const RenderDataAreaLight*> {};
        candidate_area_shadow_lights.reserve(TBX_MAX_AREA_SHADOWS);
        for (const auto& light : render_data.area_lights)
        {
            if (!light.light.cast_shadows)
                continue;
            candidate_area_shadow_lights.push_back(&light);
            if (candidate_area_shadow_lights.size() >= TBX_MAX_AREA_SHADOWS)
                break;
        }

        if (directional_shadow_light == nullptr && candidate_point_shadow_lights.empty()
            && candidate_spot_shadow_lights.empty() && candidate_area_shadow_lights.empty())
        {
            if (const auto result = ensure_shadow_resources(backend, render_data, 0U, 0U, 0U, 0U);
                !result)
            {
                return result;
            }
            return {};
        }

        auto& resource_manager = _resource_manager.get();
        auto shadow_casters = std::vector<ShadowCasterDraw> {};
        shadow_casters.reserve(render_data.renderables.size());
        auto prepare_result = Result {};

        for (const auto& renderable : render_data.renderables)
        {
            if (!prepare_result)
                break;

            auto material_resource = GraphicsMaterialInstanceResource {};
            prepare_result =
                resource_manager.upload(renderable.material, material_resource);
            if (!prepare_result)
                break;
            if (!material_casts_standard_shadow(material_resource.config))
                continue;

            const bool ignore_caster_distance =
                material_resource.config.shadow_mode == ShadowMode::Always;
            if (!ignore_caster_distance
                && !shadow_caster_within_max_camera_distance(
                    render_data.camera_position,
                    renderable.world_bounds,
                    render_data.shadow_caster_max_distance))
                continue;

            const Mat4 model_to_world = build_transform_matrix(renderable.transform);
            if (renderable.geometry_source == RenderDataGeometrySource::DynamicMesh)
            {
                Uuid vertex_buffer = {};
                Uuid index_buffer = {};
                uint32 index_count = 0U;
                prepare_result = ensure_dynamic_mesh_buffers(
                    backend,
                    renderable.dynamic_mesh,
                    vertex_buffer,
                    index_buffer,
                    index_count);
                if (!prepare_result || !vertex_buffer.is_valid())
                    continue;

                shadow_casters.push_back(
                    ShadowCasterDraw {
                        .base_batch_key = make_dynamic_batch_key(
                            make_mesh_cache_key(renderable.dynamic_mesh),
                            0U),
                        .vertex_buffer = vertex_buffer,
                        .index_buffer = index_buffer,
                        .index_count = index_count,
                        .model_to_world = model_to_world,
                        .world_bounds = renderable.world_bounds,
                    });
                continue;
            }

            if (!renderable.static_mesh.is_valid())
                continue;

            auto model_resource = GraphicsModelResource {};
            prepare_result = resource_manager.upload(
                renderable.static_mesh,
                ModelLoadParameters {},
                model_resource);
            if (!prepare_result)
                break;

            for (const auto& mesh : model_resource.meshes)
            {
                shadow_casters.push_back(
                    ShadowCasterDraw {
                        .base_batch_key = make_static_batch_key(
                            mesh.vertex_buffer,
                            mesh.index_buffer,
                            mesh.index_count,
                            0U),
                        .vertex_buffer = mesh.vertex_buffer,
                        .index_buffer = mesh.index_buffer,
                        .index_count = mesh.index_count,
                        .model_to_world = model_to_world,
                        .world_bounds = renderable.world_bounds,
                    });
            }
        }

        if (!prepare_result)
            return prepare_result;

        if (shadow_casters.empty())
        {
            if (const auto result = ensure_shadow_resources(backend, render_data, 0U, 0U, 0U, 0U);
                !result)
            {
                return result;
            }
            return {};
        }

        const uint32 preview_shadow_resolution =
            clamp_shadow_resolution(render_data.shadow_map_resolution);
        auto preview_directional_cascades = std::vector<RenderDataDirectionalShadowCascade> {};
        if (directional_shadow_light != nullptr)
        {
            const auto splits = make_cascade_splits(render_data);
            float split_near = std::max(render_data.camera.get_z_near(), 0.05F);
            float previous_split_far = split_near;
            for (uint32 cascade_index = 0U; cascade_index < TBX_SHADOW_CASCADE_COUNT;
                 ++cascade_index)
            {
                const float split_far = splits[cascade_index];
                preview_directional_cascades.push_back(make_shadow_cascade(
                    render_data,
                    *directional_shadow_light,
                    split_near,
                    split_far,
                    previous_split_far,
                    preview_shadow_resolution,
                    {}));
                previous_split_far = split_far;
                split_near = split_far;
            }
        }

        bool has_directional_casters = false;
        if (!preview_directional_cascades.empty())
        {
            for (const auto& cascade : preview_directional_cascades)
            {
                const Frustum cascade_frustum = Frustum(cascade.light_view_projection);
                const bool cascade_has_casters = std::any_of(
                    shadow_casters.begin(),
                    shadow_casters.end(),
                    [&cascade_frustum](const ShadowCasterDraw& caster)
                    {
                        return cascade_frustum.intersects(caster.world_bounds);
                    });
                if (!cascade_has_casters)
                    continue;

                has_directional_casters = true;
                break;
            }
        }

        auto kept_point_shadow_lights = std::vector<const RenderDataPointLight*> {};
        kept_point_shadow_lights.reserve(candidate_point_shadow_lights.size());
        for (const auto* light : candidate_point_shadow_lights)
        {
            const float range = std::max(light->light.range, 0.0F);
            if (range <= 0.0001F)
                continue;

            const bool contributes = std::any_of(
                shadow_casters.begin(),
                shadow_casters.end(),
                [light, range](const ShadowCasterDraw& caster)
                {
                    return shadow_caster_intersects_sphere(
                        caster.world_bounds,
                        light->transform.position,
                        range);
                });
            if (contributes)
                kept_point_shadow_lights.push_back(light);
        }

        auto kept_spot_shadow_lights = std::vector<const RenderDataSpotLight*> {};
        kept_spot_shadow_lights.reserve(candidate_spot_shadow_lights.size());
        for (const auto* light : candidate_spot_shadow_lights)
        {
            const auto preview_map = make_spot_shadow_map(*light, 0U, _shadow_resolution);
            const Frustum light_frustum = Frustum(preview_map.light_view_projection);
            const bool contributes = std::any_of(
                shadow_casters.begin(),
                shadow_casters.end(),
                [&light_frustum](const ShadowCasterDraw& caster)
                {
                    return light_frustum.intersects(caster.world_bounds);
                });
            if (contributes)
                kept_spot_shadow_lights.push_back(light);
        }

        auto kept_area_shadow_lights = std::vector<const RenderDataAreaLight*> {};
        kept_area_shadow_lights.reserve(candidate_area_shadow_lights.size());
        for (const auto* light : candidate_area_shadow_lights)
        {
            const auto preview_map = make_area_shadow_map(*light, 0U, _shadow_resolution);
            const Frustum light_frustum = Frustum(preview_map.light_view_projection);
            const bool contributes = std::any_of(
                shadow_casters.begin(),
                shadow_casters.end(),
                [&light_frustum](const ShadowCasterDraw& caster)
                {
                    return light_frustum.intersects(caster.world_bounds);
                });
            if (contributes)
                kept_area_shadow_lights.push_back(light);
        }

        const uint32 directional_shadow_count =
            has_directional_casters ? TBX_SHADOW_CASCADE_COUNT : 0U;
        const uint32 point_shadow_count = static_cast<uint32>(kept_point_shadow_lights.size());
        const uint32 spot_shadow_count = static_cast<uint32>(kept_spot_shadow_lights.size());
        const uint32 area_shadow_count = static_cast<uint32>(kept_area_shadow_lights.size());
        if (directional_shadow_count == 0U && point_shadow_count == 0U && spot_shadow_count == 0U
            && area_shadow_count == 0U)
        {
            if (const auto result = ensure_shadow_resources(backend, render_data, 0U, 0U, 0U, 0U);
                !result)
            {
                return result;
            }
            return {};
        }

        if (directional_shadow_count > 0U || spot_shadow_count > 0U || area_shadow_count > 0U)
        {
            if (const auto result = ensure_shadow_pipeline(backend); !result)
                return result;
        }
        if (point_shadow_count > 0U)
        {
            if (const auto result = ensure_point_shadow_pipeline(backend); !result)
                return result;
        }

        if (const auto result = ensure_shadow_resources(
                backend,
                render_data,
                directional_shadow_count,
                point_shadow_count,
                spot_shadow_count,
                area_shadow_count);
            !result)
        {
            return result;
        }

        render_data.directional_shadow_texture = _directional_shadow_texture;
        render_data.point_shadow_texture = _point_shadow_texture;
        render_data.spot_shadow_texture = _spot_shadow_texture;
        render_data.area_shadow_texture = _area_shadow_texture;

        if (directional_shadow_count > 0U)
        {
            render_data.directional_shadow_light_entity = directional_shadow_light->entity_uuid;
            const auto splits = make_cascade_splits(render_data);
            float split_near = std::max(render_data.camera.get_z_near(), 0.05F);
            float previous_split_far = split_near;
            for (uint32 cascade_index = 0U; cascade_index < TBX_SHADOW_CASCADE_COUNT;
                 ++cascade_index)
            {
                const float split_far = splits[cascade_index];
                render_data.directional_shadow_cascades.push_back(make_shadow_cascade(
                    render_data,
                    *directional_shadow_light,
                    split_near,
                    split_far,
                    previous_split_far,
                    _shadow_resolution,
                    _directional_shadow_texture));
                previous_split_far = split_far;
                split_near = split_far;
            }
        }

        for (uint32 index = 0U; index < kept_point_shadow_lights.size(); ++index)
        {
            render_data.point_shadow_maps.push_back(make_point_shadow_map(
                *kept_point_shadow_lights[index],
                index * 2U,
                _shadow_resolution));
        }

        for (uint32 index = 0U; index < kept_spot_shadow_lights.size(); ++index)
        {
            render_data.spot_shadow_maps.push_back(
                make_spot_shadow_map(*kept_spot_shadow_lights[index], index, _shadow_resolution));
        }

        for (uint32 index = 0U; index < kept_area_shadow_lights.size(); ++index)
        {
            render_data.area_shadow_maps.push_back(
                make_area_shadow_map(*kept_area_shadow_lights[index], index, _shadow_resolution));
        }

        const auto build_frustum_pass_batches =
            [this, &backend, &shadow_casters](
                const Frustum& frustum,
                const uint64 pass_key,
                std::unordered_map<uint64, ShadowBatch>& out_batches) -> Result
        {
            out_batches.clear();
            for (const auto& caster : shadow_casters)
            {
                if (!frustum.intersects(caster.world_bounds))
                    continue;

                auto& batch = out_batches[caster.base_batch_key];
                batch.vertex_buffer = caster.vertex_buffer;
                batch.index_buffer = caster.index_buffer;
                batch.index_count = caster.index_count;
                batch.transforms.push_back(caster.model_to_world);
            }

            for (auto& [base_key, batch] : out_batches)
            {
                if (batch.transforms.empty())
                    continue;
                if (const auto result = ensure_instance_buffer(
                        backend,
                        make_shadow_pass_batch_key(base_key, pass_key),
                        batch.transforms,
                        batch.instance_buffer);
                    !result)
                {
                    return result;
                }
            }

            return {};
        };

        const auto build_point_pass_batches =
            [this, &backend, &shadow_casters](
                const Vec3& light_center,
                const float light_range,
                const uint64 pass_key,
                std::unordered_map<uint64, ShadowBatch>& out_batches) -> Result
        {
            out_batches.clear();
            for (const auto& caster : shadow_casters)
            {
                if (!shadow_caster_intersects_sphere(
                        caster.world_bounds,
                        light_center,
                        light_range))
                    continue;

                auto& batch = out_batches[caster.base_batch_key];
                batch.vertex_buffer = caster.vertex_buffer;
                batch.index_buffer = caster.index_buffer;
                batch.index_count = caster.index_count;
                batch.transforms.push_back(caster.model_to_world);
            }

            for (auto& [base_key, batch] : out_batches)
            {
                if (batch.transforms.empty())
                    continue;
                if (const auto result = ensure_instance_buffer(
                        backend,
                        make_shadow_pass_batch_key(base_key, pass_key),
                        batch.transforms,
                        batch.instance_buffer);
                    !result)
                {
                    return result;
                }
            }

            return {};
        };

        if (_shadow_view_uniform_buffers.size() < render_data.directional_shadow_cascades.size())
            _shadow_view_uniform_buffers.resize(render_data.directional_shadow_cascades.size());

        for (uint32 cascade_index = 0U;
             cascade_index < render_data.directional_shadow_cascades.size();
             ++cascade_index)
        {
            auto& cascade = render_data.directional_shadow_cascades[cascade_index];
            Uuid& view_buffer = _shadow_view_uniform_buffers[cascade_index];
            const auto data_size = static_cast<uint64>(sizeof(Mat4));
            if (!view_buffer.is_valid())
            {
                if (const auto result = _resource_manager.get().upload(
                        GraphicsBufferDesc {
                            .usage = GraphicsBufferUsage::UNIFORM,
                            .size = data_size,
                            .is_dynamic = true,
                            .debug_name = "Toybox Directional Shadow View Uniforms",
                        },
                        &cascade.light_view_projection,
                        data_size,
                        view_buffer);
                    !result)
                    return result;
            }
            else if (
                const auto result =
                    _resource_manager.get()
                        .update(view_buffer, &cascade.light_view_projection, data_size, 0U);
                !result)
            {
                return result;
            }

            auto pass_batches = std::unordered_map<uint64, ShadowBatch> {};
            if (const auto result = build_frustum_pass_batches(
                    Frustum(cascade.light_view_projection),
                    0x10000000ULL + static_cast<uint64>(cascade_index),
                    pass_batches);
                !result)
            {
                return result;
            }

            auto pass = GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .depth_stencil_target = _directional_shadow_texture,
                        .depth_stencil_layer = static_cast<int32>(cascade_index),
                        .clear_depth = 1.0F,
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .debug_name = "Toybox Directional Shadow Pass",
                    },
                .viewport =
                    Viewport {
                        .position = Vec2(0.0F),
                        .dimensions = Size {_shadow_resolution, _shadow_resolution},
                    },
            };

            for (const auto& [base_key, batch] : pass_batches)
            {
                (void)base_key;
                if (!batch.vertex_buffer.is_valid() || !batch.instance_buffer.is_valid()
                    || batch.transforms.empty())
                    continue;

                pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = _shadow_pipeline,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = batch.vertex_buffer},
                                GraphicsResourceBinding {
                                    .slot = 1U,
                                    .resource = batch.instance_buffer},
                            },
                        .index_buffer = batch.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .uniform_buffers =
                            {
                                GraphicsResourceBinding {.slot = 0U, .resource = view_buffer},
                            },
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = batch.index_count,
                                .instance_count = static_cast<uint32>(batch.transforms.size()),
                            },
                    });
            }

            render_data.directional_shadow_passes.push_back(std::move(pass));
        }

        if (_spot_shadow_view_uniform_buffers.size() < render_data.spot_shadow_maps.size())
            _spot_shadow_view_uniform_buffers.resize(render_data.spot_shadow_maps.size());
        for (uint32 map_index = 0U; map_index < render_data.spot_shadow_maps.size(); ++map_index)
        {
            const auto& shadow_map = render_data.spot_shadow_maps[map_index];
            Uuid& view_buffer = _spot_shadow_view_uniform_buffers[map_index];
            const auto data_size = static_cast<uint64>(sizeof(Mat4));
            if (!view_buffer.is_valid())
            {
                if (const auto result = _resource_manager.get().upload(
                        GraphicsBufferDesc {
                            .usage = GraphicsBufferUsage::UNIFORM,
                            .size = data_size,
                            .is_dynamic = true,
                            .debug_name = "Toybox Spot Shadow View Uniforms",
                        },
                        &shadow_map.light_view_projection,
                        data_size,
                        view_buffer);
                    !result)
                {
                    return result;
                }
            }
            else if (
                const auto result =
                    _resource_manager.get()
                        .update(view_buffer, &shadow_map.light_view_projection, data_size, 0U);
                !result)
            {
                return result;
            }

            auto pass_batches = std::unordered_map<uint64, ShadowBatch> {};
            if (const auto result = build_frustum_pass_batches(
                    Frustum(shadow_map.light_view_projection),
                    0x20000000ULL + static_cast<uint64>(map_index),
                    pass_batches);
                !result)
            {
                return result;
            }

            auto pass = GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .depth_stencil_target = _spot_shadow_texture,
                        .depth_stencil_layer = static_cast<int32>(shadow_map.texture_layer),
                        .clear_depth = 1.0F,
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .debug_name = "Toybox Spot Shadow Pass",
                    },
                .viewport =
                    Viewport {
                        .position = Vec2(0.0F),
                        .dimensions = Size {_shadow_resolution, _shadow_resolution},
                    },
            };

            for (const auto& [base_key, batch] : pass_batches)
            {
                (void)base_key;
                if (!batch.vertex_buffer.is_valid() || !batch.instance_buffer.is_valid()
                    || batch.transforms.empty())
                    continue;

                pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = _shadow_pipeline,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = batch.vertex_buffer},
                                GraphicsResourceBinding {
                                    .slot = 1U,
                                    .resource = batch.instance_buffer},
                            },
                        .index_buffer = batch.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .uniform_buffers =
                            {
                                GraphicsResourceBinding {.slot = 0U, .resource = view_buffer},
                            },
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = batch.index_count,
                                .instance_count = static_cast<uint32>(batch.transforms.size()),
                            },
                    });
            }

            render_data.spot_shadow_passes.push_back(std::move(pass));
        }

        if (_area_shadow_view_uniform_buffers.size() < render_data.area_shadow_maps.size())
            _area_shadow_view_uniform_buffers.resize(render_data.area_shadow_maps.size());
        for (uint32 map_index = 0U; map_index < render_data.area_shadow_maps.size(); ++map_index)
        {
            const auto& shadow_map = render_data.area_shadow_maps[map_index];
            Uuid& view_buffer = _area_shadow_view_uniform_buffers[map_index];
            const auto data_size = static_cast<uint64>(sizeof(Mat4));
            if (!view_buffer.is_valid())
            {
                if (const auto result = _resource_manager.get().upload(
                        GraphicsBufferDesc {
                            .usage = GraphicsBufferUsage::UNIFORM,
                            .size = data_size,
                            .is_dynamic = true,
                            .debug_name = "Toybox Area Shadow View Uniforms",
                        },
                        &shadow_map.light_view_projection,
                        data_size,
                        view_buffer);
                    !result)
                {
                    return result;
                }
            }
            else if (
                const auto result =
                    _resource_manager.get()
                        .update(view_buffer, &shadow_map.light_view_projection, data_size, 0U);
                !result)
            {
                return result;
            }

            auto pass_batches = std::unordered_map<uint64, ShadowBatch> {};
            if (const auto result = build_frustum_pass_batches(
                    Frustum(shadow_map.light_view_projection),
                    0x30000000ULL + static_cast<uint64>(map_index),
                    pass_batches);
                !result)
            {
                return result;
            }

            auto pass = GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .depth_stencil_target = _area_shadow_texture,
                        .depth_stencil_layer = static_cast<int32>(shadow_map.texture_layer),
                        .clear_depth = 1.0F,
                        .clear_flags = GraphicsClearFlags::DEPTH,
                        .debug_name = "Toybox Area Shadow Pass",
                    },
                .viewport =
                    Viewport {
                        .position = Vec2(0.0F),
                        .dimensions = Size {_shadow_resolution, _shadow_resolution},
                    },
            };

            for (const auto& [base_key, batch] : pass_batches)
            {
                (void)base_key;
                if (!batch.vertex_buffer.is_valid() || !batch.instance_buffer.is_valid()
                    || batch.transforms.empty())
                    continue;

                pass.indexed_draws.push_back(
                    GraphicsIndexedDrawCommand {
                        .pipeline = _shadow_pipeline,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = batch.vertex_buffer},
                                GraphicsResourceBinding {
                                    .slot = 1U,
                                    .resource = batch.instance_buffer},
                            },
                        .index_buffer = batch.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .uniform_buffers =
                            {
                                GraphicsResourceBinding {.slot = 0U, .resource = view_buffer},
                            },
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = batch.index_count,
                                .instance_count = static_cast<uint32>(batch.transforms.size()),
                            },
                    });
            }

            render_data.area_shadow_passes.push_back(std::move(pass));
        }

        const uint32 point_shadow_pass_count =
            static_cast<uint32>(render_data.point_shadow_maps.size()) * 2U;
        if (_point_shadow_uniform_buffers.size() < point_shadow_pass_count)
            _point_shadow_uniform_buffers.resize(point_shadow_pass_count);
        for (uint32 map_index = 0U; map_index < render_data.point_shadow_maps.size(); ++map_index)
        {
            const auto& shadow_map = render_data.point_shadow_maps[map_index];
            const Vec3 light_center = kept_point_shadow_lights[map_index]->transform.position;
            auto pass_batches = std::unordered_map<uint64, ShadowBatch> {};
            if (const auto result = build_point_pass_batches(
                    light_center,
                    shadow_map.range,
                    0x40000000ULL + static_cast<uint64>(map_index),
                    pass_batches);
                !result)
            {
                return result;
            }

            for (uint32 hemisphere = 0U; hemisphere < 2U; ++hemisphere)
            {
                const uint32 pass_index = (map_index * 2U) + hemisphere;
                Uuid& pass_uniform_buffer = _point_shadow_uniform_buffers[pass_index];
                const auto uniforms = PointShadowPassUniform {
                    .world_to_light = shadow_map.world_to_light,
                    .shadow_params = Vec4(
                        shadow_map.range,
                        hemisphere == 0U ? 1.0F : -1.0F,
                        shadow_map.normal_bias,
                        shadow_map.depth_bias),
                };
                const auto data_size = static_cast<uint64>(sizeof(PointShadowPassUniform));
                if (!pass_uniform_buffer.is_valid())
                {
                    if (const auto result = _resource_manager.get().upload(
                            GraphicsBufferDesc {
                                .usage = GraphicsBufferUsage::UNIFORM,
                                .size = data_size,
                                .is_dynamic = true,
                                .debug_name = "Toybox Point Shadow Uniforms",
                            },
                            &uniforms,
                            data_size,
                            pass_uniform_buffer);
                        !result)
                    {
                        return result;
                    }
                }
                else if (
                    const auto result = _resource_manager.get()
                                            .update(pass_uniform_buffer, &uniforms, data_size, 0U);
                    !result)
                {
                    return result;
                }

                auto pass = GraphicsRenderPass {
                    .pass =
                        GraphicsPassDesc {
                            .depth_stencil_target = _point_shadow_texture,
                            .depth_stencil_layer =
                                static_cast<int32>(shadow_map.layer_offset + hemisphere),
                            .clear_depth = 1.0F,
                            .clear_flags = GraphicsClearFlags::DEPTH,
                            .debug_name = "Toybox Point Shadow Pass",
                        },
                    .viewport =
                        Viewport {
                            .position = Vec2(0.0F),
                            .dimensions = Size {_shadow_resolution, _shadow_resolution},
                        },
                };

                for (const auto& [base_key, batch] : pass_batches)
                {
                    (void)base_key;
                    if (!batch.vertex_buffer.is_valid() || !batch.instance_buffer.is_valid()
                        || batch.transforms.empty())
                        continue;

                    pass.indexed_draws.push_back(
                        GraphicsIndexedDrawCommand {
                            .pipeline = _point_shadow_pipeline,
                            .vertex_buffers =
                                {
                                    GraphicsResourceBinding {
                                        .slot = 0U,
                                        .resource = batch.vertex_buffer},
                                    GraphicsResourceBinding {
                                        .slot = 1U,
                                        .resource = batch.instance_buffer},
                                },
                            .index_buffer = batch.index_buffer,
                            .index_type = GraphicsIndexType::UINT32,
                            .uniform_buffers =
                                {
                                    GraphicsResourceBinding {
                                        .slot = 1U,
                                        .resource = pass_uniform_buffer},
                                },
                            .draw =
                                GraphicsDrawIndexedDesc {
                                    .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                    .index_type = GraphicsIndexType::UINT32,
                                    .index_count = batch.index_count,
                                    .instance_count = static_cast<uint32>(batch.transforms.size()),
                                },
                        });
                }

                render_data.point_shadow_passes.push_back(std::move(pass));
            }
        }

        return {};
    }

    Result DirectionalShadowPassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        if (const auto result = execute_render_pass_list(
                backend,
                render_data,
                render_data.directional_shadow_passes,
                token,
                "DirectionalShadowPassOperation (directional)");
            !result)
        {
            return result;
        }

        if (const auto result = execute_render_pass_list(
                backend,
                render_data,
                render_data.point_shadow_passes,
                token,
                "DirectionalShadowPassOperation (point)");
            !result)
        {
            return result;
        }

        if (const auto result = execute_render_pass_list(
                backend,
                render_data,
                render_data.spot_shadow_passes,
                token,
                "DirectionalShadowPassOperation (spot)");
            !result)
        {
            return result;
        }

        return execute_render_pass_list(
            backend,
            render_data,
            render_data.area_shadow_passes,
            token,
            "DirectionalShadowPassOperation (area)");
    }

}
