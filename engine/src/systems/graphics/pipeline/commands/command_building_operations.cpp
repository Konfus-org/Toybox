#include "tbx/systems/graphics/pipeline/commands/command_building_operations.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/frustum.h"
#include "tbx/types/components/mesh.h"
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
    BuildSkyboxCommandsOperation::BuildSkyboxCommandsOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo BuildSkyboxCommandsOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Build Skybox Commands Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    Result BuildSkyboxCommandsOperation::ensure_geometry(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid() && _index_buffer.is_valid() && _index_count > 0U)
            return {};

        const Mesh& mesh = sky_dome;
        const uint64 vertex_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));

        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Sky Dome Vertices",
                },
                mesh.vertices.data(),
                vertex_size,
                _vertex_buffer);
            !result)
            return result;

        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Sky Dome Indices",
                },
                mesh.indices.data(),
                index_size,
                _index_buffer);
            !result)
        {
            backend.unload(_vertex_buffer);
            _vertex_buffer = {};
            return result;
        }

        _index_count = static_cast<uint32>(mesh.indices.size());
        return {};
    }

    Result BuildSkyboxCommandsOperation::ensure_instance_buffer(
        IGraphicsBackend& backend,
        const Vec3& camera_position,
        const Transform& sky_transform)
    {
        constexpr float sky_radius = 256.0F;
        auto placed = sky_transform;
        placed.position += camera_position;
        placed.scale *= sky_radius;
        const Mat4 model = build_transform_matrix(placed);
        const auto data_size = static_cast<uint64>(sizeof(Mat4));

        if (!_instance_buffer.is_valid())
        {
            return backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = data_size,
                    .is_dynamic = true,
                    .debug_name = "Toybox Skybox Instance Transform",
                },
                &model,
                data_size,
                _instance_buffer);
        }

        return backend.update_buffer(_instance_buffer, &model, data_size, 0U);
    }

    Result BuildSkyboxCommandsOperation::ensure_material_uniform(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size)
    {
        if (_material_key == material_key && _material_uniform_buffer.is_valid())
            return {};

        if (_material_uniform_buffer.is_valid())
        {
            backend.unload(_material_uniform_buffer);
            _material_uniform_buffer = {};
        }

        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Skybox Material Uniforms",
                },
                data,
                data_size,
                _material_uniform_buffer);
            !result)
            return result;

        _material_key = material_key;
        return {};
    }

    Result BuildSkyboxCommandsOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data;
        render_data.skybox_commands.clear();
        render_data.has_skybox = false;

        const RenderData& scene_data = render_data;
        if (!scene_data.sky.sky.material.get_handle().is_valid())
            return {};

        const MaterialInstance& sky_material = scene_data.sky.sky.material;
        const Transform& sky_transform = scene_data.sky.transform;

        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
            return Result(false, "BuildSkyboxCommandsOperation requires IGraphicsBackend service.");
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();

        auto material_resource = GraphicsMaterialDrawResource {};
        if (const auto result =
                resource_manager.load_material_draw_resource(sky_material, material_resource);
            !result)
            return result;

        if (const auto result = ensure_material_uniform(
                backend,
                material_resource.uniform_key,
                material_resource.uniform_data.data(),
                material_resource.uniform_data.byte_size());
            !result)
            return result;

        if (const auto result = ensure_geometry(backend); !result)
            return result;

        if (const auto result =
                ensure_instance_buffer(backend, frame_data.camera_position, sky_transform);
            !result)
            return result;

        render_data.skybox_commands.push_back(
            GraphicsIndexedDrawCommand {
                .pipeline = material_resource.pipeline,
                .vertex_buffers =
                    {
                        GraphicsResourceBinding {.slot = 0U, .resource = _vertex_buffer},
                        GraphicsResourceBinding {.slot = 1U, .resource = _instance_buffer},
                    },
                .index_buffer = _index_buffer,
                .index_type = GraphicsIndexType::UINT32,
                .uniform_buffers =
                    {
                        GraphicsResourceBinding {
                            .slot = 0U,
                            .resource = frame_data.view_uniform_buffer},
                        GraphicsResourceBinding {.slot = 1U, .resource = _material_uniform_buffer},
                    },
                .textures = std::move(material_resource.textures),
                .draw =
                    GraphicsDrawIndexedDesc {
                        .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                        .index_type = GraphicsIndexType::UINT32,
                        .index_count = _index_count,
                        .instance_count = 1U,
                    },
            });
        render_data.has_skybox = true;
        return {};
    }

    Result BuildSkyboxCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildSkyboxCommandsOperation::release(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid())
            backend.unload(_vertex_buffer);
        if (_index_buffer.is_valid())
            backend.unload(_index_buffer);
        if (_instance_buffer.is_valid())
            backend.unload(_instance_buffer);
        if (_material_uniform_buffer.is_valid())
            backend.unload(_material_uniform_buffer);

        _vertex_buffer = {};
        _index_buffer = {};
        _index_count = 0U;
        _instance_buffer = {};
        _material_uniform_buffer = {};
        _material_key = 0U;
    }

    BuildOpaqueCommandsOperation::BuildOpaqueCommandsOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo BuildOpaqueCommandsOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Build Opaque Commands Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    // ---------------------------------------------------------------------------
    // CPU-side geometry clipping (fallback for non-standard mesh strides)
    // ---------------------------------------------------------------------------

    struct GeometryBuildResult
    {
        std::vector<float> vertices = {};
        std::vector<uint32> indices = {};
    };

    static float get_clip_plane_distance(const Vec4& position, const uint32 plane_index)
    {
        switch (plane_index)
        {
            case 0U:
                return position.x + position.w;
            case 1U:
                return position.w - position.x;
            case 2U:
                return position.y + position.w;
            case 3U:
                return position.w - position.y;
            case 4U:
                return position.z + position.w;
            case 5U:
                return position.w - position.z;
            default:
                return 0.0F;
        }
    }

    static Vec4 interpolate_clip_position(
        const Vec4& start,
        const Vec4& end,
        const float start_distance,
        const float end_distance)
    {
        const float denominator = start_distance - end_distance;
        if (std::abs(denominator) <= 0.000001F)
            return start;
        const float t = start_distance / denominator;
        return start + ((end - start) * t);
    }

    static std::vector<Vec4> clip_polygon_against_plane(
        const std::vector<Vec4>& polygon,
        const uint32 plane_index)
    {
        auto clipped = std::vector<Vec4> {};
        if (polygon.empty())
            return clipped;

        clipped.reserve(polygon.size() + 1U);
        Vec4 previous = polygon.back();
        float previous_distance = get_clip_plane_distance(previous, plane_index);
        bool previous_inside = previous_distance >= 0.0F;

        for (const Vec4& current : polygon)
        {
            const float current_distance = get_clip_plane_distance(current, plane_index);
            const bool current_inside = current_distance >= 0.0F;

            if (current_inside != previous_inside)
            {
                clipped.push_back(interpolate_clip_position(
                    previous,
                    current,
                    previous_distance,
                    current_distance));
            }
            if (current_inside)
                clipped.push_back(current);

            previous = current;
            previous_distance = current_distance;
            previous_inside = current_inside;
        }
        return clipped;
    }

    static void append_projected_vertex(const Vec4& clip_position, GeometryBuildResult& geometry)
    {
        const float inverse_w =
            std::abs(clip_position.w) <= 0.000001F ? 1.0F : 1.0F / clip_position.w;
        geometry.vertices.push_back(clip_position.x * inverse_w);
        geometry.vertices.push_back(clip_position.y * inverse_w);
        geometry.vertices.push_back(clip_position.z * inverse_w);
    }

    static void append_clipped_triangle(
        const Vec4& v0,
        const Vec4& v1,
        const Vec4& v2,
        GeometryBuildResult& geometry)
    {
        auto polygon = std::vector<Vec4> {v0, v1, v2};
        for (uint32 plane = 0U; plane < 6U; ++plane)
        {
            polygon = clip_polygon_against_plane(polygon, plane);
            if (polygon.size() < 3U)
                return;
        }

        for (uint32 i = 1U; i + 1U < polygon.size(); ++i)
        {
            const uint32 base = static_cast<uint32>(geometry.vertices.size() / 3U);
            append_projected_vertex(polygon[0U], geometry);
            append_projected_vertex(polygon[i], geometry);
            append_projected_vertex(polygon[i + 1U], geometry);
            geometry.indices.push_back(base);
            geometry.indices.push_back(base + 1U);
            geometry.indices.push_back(base + 2U);
        }
    }

    static uint32 get_vertex_stride_float_count(const Mesh& mesh)
    {
        const uint32 stride_bytes = mesh.vertices.layout.stride;
        return stride_bytes == 0U ? 16U : stride_bytes / static_cast<uint32>(sizeof(float));
    }

    static bool can_render_mesh_directly(const Mesh& mesh)
    {
        constexpr uint32 model_pipeline_stride = 16U;
        return !mesh.vertices.empty() && !mesh.indices.empty()
               && get_vertex_stride_float_count(mesh) == model_pipeline_stride;
    }

    static void append_mesh_geometry(
        const Mesh& mesh,
        const Mat4& world_to_clip,
        GeometryBuildResult& geometry)
    {
        if (mesh.vertices.empty() || mesh.indices.empty())
            return;

        const uint32 stride = get_vertex_stride_float_count(mesh);
        if (stride < 3U)
            return;

        const uint32 source_vertex_count = static_cast<uint32>(mesh.vertices.size() / stride);
        auto clip_positions = std::vector<Vec4> {};
        clip_positions.reserve(source_vertex_count);

        for (uint32 vertex_index = 0U; vertex_index < source_vertex_count; ++vertex_index)
        {
            const uint32 offset = vertex_index * stride;
            const Vec4 local = Vec4(
                mesh.vertices.vertices[offset],
                mesh.vertices.vertices[offset + 1U],
                mesh.vertices.vertices[offset + 2U],
                1.0F);
            clip_positions.push_back(world_to_clip * local);
        }

        for (uint32 index_offset = 0U; index_offset + 2U < mesh.indices.size(); index_offset += 3U)
        {
            const uint32 i0 = mesh.indices[index_offset];
            const uint32 i1 = mesh.indices[index_offset + 1U];
            const uint32 i2 = mesh.indices[index_offset + 2U];
            if (i0 >= source_vertex_count || i1 >= source_vertex_count || i2 >= source_vertex_count)
                continue;

            append_clipped_triangle(
                clip_positions[i0],
                clip_positions[i1],
                clip_positions[i2],
                geometry);
        }
    }

    // ---------------------------------------------------------------------------
    // Fallback pipeline definition
    // ---------------------------------------------------------------------------

    static Shader make_opaque_fallback_shader()
    {
        return Shader(
            std::vector<ShaderSource> {
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) in vec3 a_position;\n"
                    "out vec3 v_color;\n"
                    "void main()\n"
                    "{\n"
                    "    v_color = (a_position * 0.5) + vec3(0.5, 0.5, 0.75);\n"
                    "    gl_Position = vec4(a_position, 1.0);\n"
                    "}\n",
                    ShaderType::VERTEX),
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) out vec4 o_final_color;\n"
                    "in vec3 v_color;\n"
                    "void main()\n"
                    "{\n"
                    "    o_final_color = vec4(clamp(v_color, 0.15, 1.0), 1.0);\n"
                    "}\n",
                    ShaderType::FRAGMENT),
            });
    }

    static GraphicsPipelineDesc make_fallback_pipeline_desc()
    {
        return GraphicsPipelineDesc {
            .shader = make_opaque_fallback_shader(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = static_cast<uint32>(sizeof(float) * 3U),
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
                },
            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
            .is_depth_test_enabled = true,
            .is_depth_write_enabled = true,
            .is_blending_enabled = false,
            .is_culling_enabled = false,
            .debug_name = "Toybox Fallback Geometry Pipeline",
        };
    }

    static uint64 make_mesh_cache_key(const std::shared_ptr<Mesh>& mesh_data)
    {
        return reinterpret_cast<uint64>(mesh_data.get());
    }

    static uint64 make_dynamic_batch_key(const uint64 mesh_key, const uint64 material_key)
    {
        return fnv1a_hash_value(material_key, fnv1a_hash_value(mesh_key, TBX_FNV1A_OFFSET_BASIS));
    }

    static uint64 make_static_batch_key(
        const Uuid vertex_buffer,
        const Uuid index_buffer,
        const uint32 index_count,
        const uint64 material_key)
    {
        uint64 hash = fnv1a_hash_uuid(vertex_buffer, TBX_FNV1A_OFFSET_BASIS);
        hash = fnv1a_hash_uuid(index_buffer, hash);
        hash = fnv1a_hash_value(index_count, hash);
        hash = fnv1a_hash_value(material_key, hash);
        return hash == 0U ? 1U : hash;
    }

    // ---------------------------------------------------------------------------
    // Batch accumulation structures
    // ---------------------------------------------------------------------------

    struct DynamicBatch
    {
        std::shared_ptr<Mesh> mesh_data = {};
        Uuid pipeline = {};
        uint64 material_key = 0U;
        Uuid material_uniform_buffer = {};
        std::vector<GraphicsResourceBinding> textures = {};
        std::vector<Mat4> transforms = {};
    };

    struct StaticBatch
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        uint32 index_count = 0U;
        Uuid pipeline = {};
        uint64 material_key = 0U;
        Uuid material_uniform_buffer = {};
        std::vector<GraphicsResourceBinding> textures = {};
        std::vector<Mat4> transforms = {};
    };

    inline constexpr uint32 TBX_MAX_DIRECTIONAL_LIGHTS = 4U;
    inline constexpr uint32 TBX_MAX_POINT_LIGHTS = 16U;
    inline constexpr uint32 TBX_MAX_SPOT_LIGHTS = 8U;
    inline constexpr uint32 TBX_MAX_AREA_LIGHTS = 4U;
    inline constexpr uint32 TBX_SHADOW_CASCADE_COUNT = 3U;
    inline constexpr uint32 TBX_MAX_POINT_SHADOWS = TBX_MAX_POINT_LIGHTS;
    inline constexpr uint32 TBX_MAX_SPOT_SHADOWS = TBX_MAX_SPOT_LIGHTS;
    inline constexpr uint32 TBX_MAX_AREA_SHADOWS = TBX_MAX_AREA_LIGHTS;

    static Vec3 make_light_direction(const Transform& transform)
    {
        return normalize(transform.rotation * Vec3(0.0F, 0.0F, -1.0F));
    }

    static Vec4 make_light_radiance(const Light& light)
    {
        const float strongest_channel =
            std::max(light.color.r, std::max(light.color.g, light.color.b));
        const float normalization = strongest_channel <= 0.0001F ? 0.0F : 1.0F / strongest_channel;
        return Vec4(
            light.color.r * normalization * light.intensity,
            light.color.g * normalization * light.intensity,
            light.color.b * normalization * light.intensity,
            light.intensity);
    }

    static float angle_to_cosine(const float angle_degrees)
    {
        constexpr float degrees_to_radians = 0.017453292519943295F;
        return std::cos(angle_degrees * degrees_to_radians);
    }

    // ---------------------------------------------------------------------------
    // BuildDirectionalShadowCommandsOperation implementation
    // ---------------------------------------------------------------------------

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
                    "layout(std140, binding = 0) uniform ToyboxViewBlock\n"
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
                    "layout(std140, binding = 0) uniform ToyboxPointShadowBlock\n"
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
        const Vec3 right =
            normalize(frame_data.camera_transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
        const Vec3 up =
            normalize(frame_data.camera_transform.rotation * Vec3(0.0F, 1.0F, 0.0F));

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
        const float texel_world =
            z_far / static_cast<float>(std::max(shadow_resolution, 1U));

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
        const float texel_world =
            z_far / static_cast<float>(std::max(shadow_resolution, 1U));

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

    BuildDirectionalShadowCommandsOperation::BuildDirectionalShadowCommandsOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo BuildDirectionalShadowCommandsOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Build Directional Shadow Commands Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    Result BuildDirectionalShadowCommandsOperation::ensure_shadow_pipeline(
        IGraphicsBackend& backend)
    {
        if (_shadow_pipeline.is_valid())
            return {};
        return backend.upload_pipeline(make_directional_shadow_pipeline_desc(), _shadow_pipeline);
    }

    Result BuildDirectionalShadowCommandsOperation::ensure_point_shadow_pipeline(
        IGraphicsBackend& backend)
    {
        if (_point_shadow_pipeline.is_valid())
            return {};
        return backend.upload_pipeline(make_point_shadow_pipeline_desc(), _point_shadow_pipeline);
    }

    Result BuildDirectionalShadowCommandsOperation::ensure_shadow_resources(
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
                backend.unload(_directional_shadow_texture);
            _directional_shadow_texture = {};
        }
        else if (resolution_changed || !_directional_shadow_texture.is_valid())
        {
            if (_directional_shadow_texture.is_valid())
                backend.unload(_directional_shadow_texture);
            _directional_shadow_texture = {};

            if (const auto result = backend.upload_texture(
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
                backend.unload(_point_shadow_texture);
            if (_spot_shadow_texture.is_valid())
                backend.unload(_spot_shadow_texture);
            if (_area_shadow_texture.is_valid())
                backend.unload(_area_shadow_texture);
            _point_shadow_texture = {};
            _spot_shadow_texture = {};
            _area_shadow_texture = {};
            _point_shadow_texture_layers = 0U;
            _spot_shadow_texture_layers = 0U;
            _area_shadow_texture_layers = 0U;
        }

        auto ensure_array_shadow_texture = [&backend, resolution](
                                             const uint32 required_layers,
                                             Uuid& texture,
                                             uint32& capacity_layers,
                                             const std::string& name) -> Result
        {
            if (required_layers == 0U)
            {
                if (texture.is_valid())
                    backend.unload(texture);
                texture = {};
                capacity_layers = 0U;
                return {};
            }

            if (texture.is_valid() && capacity_layers >= required_layers)
                return {};

            if (texture.is_valid())
            {
                backend.unload(texture);
                texture = {};
                capacity_layers = 0U;
            }

            auto created = Uuid {};
            if (const auto result = backend.upload_texture(
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

        return {};
    }

    Result BuildDirectionalShadowCommandsOperation::ensure_dynamic_mesh_buffers(
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
            return {};
        }

        const uint64 vertex_size =
            static_cast<uint64>(mesh->vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh->indices.size()) * static_cast<uint64>(sizeof(uint32));

        auto vertex_buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
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
        if (const auto result = backend.upload_buffer(
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
            backend.unload(vertex_buffer);
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

    Result BuildDirectionalShadowCommandsOperation::ensure_instance_buffer(
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
                backend.unload(buffer_it->second);

            auto buffer = Uuid {};
            if (const auto result = backend.upload_buffer(
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
                backend.update_buffer(buffer_it->second, transforms.data(), data_size, 0U);
            !result)
            return result;

        out_buffer = buffer_it->second;
        return {};
    }

    Result BuildDirectionalShadowCommandsOperation::prepare(RenderData& render_data)
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
                "BuildDirectionalShadowCommandsOperation requires IGraphicsBackend service.");
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
                resource_manager.load_material_instance(renderable.material, material_resource);
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
            prepare_result = resource_manager.load_model(renderable.static_mesh, model_resource);
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

        const uint32 preview_shadow_resolution = clamp_shadow_resolution(
            render_data.shadow_map_resolution);
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

        const uint32 directional_shadow_count = has_directional_casters ? TBX_SHADOW_CASCADE_COUNT : 0U;
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
                if (!shadow_caster_intersects_sphere(caster.world_bounds, light_center, light_range))
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
                if (const auto result = backend.upload_buffer(
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
            else if (const auto result = backend.update_buffer(
                         view_buffer,
                         &cascade.light_view_projection,
                         data_size,
                         0U);
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
                if (const auto result = backend.upload_buffer(
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
            else if (const auto result = backend.update_buffer(
                         view_buffer,
                         &shadow_map.light_view_projection,
                         data_size,
                         0U);
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
                if (const auto result = backend.upload_buffer(
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
            else if (const auto result = backend.update_buffer(
                         view_buffer,
                         &shadow_map.light_view_projection,
                         data_size,
                         0U);
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
            const Vec3 light_center =
                kept_point_shadow_lights[map_index]->transform.position;
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
                    if (const auto result = backend.upload_buffer(
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
                else if (const auto result = backend.update_buffer(
                             pass_uniform_buffer,
                             &uniforms,
                             data_size,
                             0U);
                         !result)
                {
                    return result;
                }

                auto pass = GraphicsRenderPass {
                    .pass =
                        GraphicsPassDesc {
                            .depth_stencil_target = _point_shadow_texture,
                            .depth_stencil_layer = static_cast<int32>(
                                shadow_map.layer_offset + hemisphere),
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
                                        .slot = 0U,
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

    Result BuildDirectionalShadowCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildDirectionalShadowCommandsOperation::release(IGraphicsBackend& backend)
    {
        for (const auto& [key, buffer] : _mesh_vertex_buffers)
            backend.unload(buffer);
        for (const auto& [key, buffer] : _mesh_index_buffers)
            backend.unload(buffer);
        for (const auto& [key, buffer] : _instance_buffers)
            backend.unload(buffer);
        if (_directional_shadow_texture.is_valid())
            backend.unload(_directional_shadow_texture);
        for (const Uuid& buffer : _shadow_view_uniform_buffers)
            if (buffer.is_valid())
                backend.unload(buffer);
        for (const Uuid& buffer : _spot_shadow_view_uniform_buffers)
            if (buffer.is_valid())
                backend.unload(buffer);
        for (const Uuid& buffer : _area_shadow_view_uniform_buffers)
            if (buffer.is_valid())
                backend.unload(buffer);
        for (const Uuid& buffer : _point_shadow_uniform_buffers)
            if (buffer.is_valid())
                backend.unload(buffer);
        if (_point_shadow_texture.is_valid())
            backend.unload(_point_shadow_texture);
        if (_spot_shadow_texture.is_valid())
            backend.unload(_spot_shadow_texture);
        if (_area_shadow_texture.is_valid())
            backend.unload(_area_shadow_texture);
        if (_shadow_pipeline.is_valid())
            backend.unload(_shadow_pipeline);
        if (_point_shadow_pipeline.is_valid())
            backend.unload(_point_shadow_pipeline);

        _mesh_vertex_buffers.clear();
        _mesh_index_buffers.clear();
        _mesh_index_counts.clear();
        _instance_buffers.clear();
        _instance_buffer_sizes.clear();
        _directional_shadow_texture = {};
        _shadow_view_uniform_buffers.clear();
        _spot_shadow_view_uniform_buffers.clear();
        _area_shadow_view_uniform_buffers.clear();
        _point_shadow_uniform_buffers.clear();
        _shadow_pipeline = {};
        _point_shadow_pipeline = {};
        _point_shadow_texture = {};
        _spot_shadow_texture = {};
        _area_shadow_texture = {};
        _shadow_resolution = 0U;
        _point_shadow_texture_layers = 0U;
        _spot_shadow_texture_layers = 0U;
        _area_shadow_texture_layers = 0U;
    }

    // ---------------------------------------------------------------------------
    // BuildOpaqueCommandsOperation implementation
    // ---------------------------------------------------------------------------

    Result BuildOpaqueCommandsOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (material_key == 0U || data == nullptr || data_size == 0U)
            return Result(false, "BuildOpaqueCommandsOperation: invalid material uniform data.");

        const auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Material Uniforms",
                },
                data,
                data_size,
                buffer);
            !result)
            return result;

        _material_uniform_buffers[material_key] = buffer;
        out_buffer = buffer;
        return {};
    }

    Result BuildOpaqueCommandsOperation::ensure_dynamic_mesh_buffers(
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
            return Result(false, "BuildOpaqueCommandsOperation: mesh is not directly renderable.");

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
            return {};
        }

        const uint64 vertex_size =
            static_cast<uint64>(mesh->vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh->indices.size()) * static_cast<uint64>(sizeof(uint32));

        auto vertex_buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Dynamic Mesh Vertices",
                },
                mesh->vertices.data(),
                vertex_size,
                vertex_buffer);
            !result)
            return result;

        auto index_buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Dynamic Mesh Indices",
                },
                mesh->indices.data(),
                index_size,
                index_buffer);
            !result)
        {
            backend.unload(vertex_buffer);
            return result;
        }

        _mesh_sources[mesh_key] = mesh;
        _mesh_vertex_buffers[mesh_key] = vertex_buffer;
        _mesh_index_buffers[mesh_key] = index_buffer;
        _mesh_index_counts[mesh_key] = static_cast<uint32>(mesh->indices.size());
        out_vertex_buffer = vertex_buffer;
        out_index_buffer = index_buffer;
        out_index_count = static_cast<uint32>(mesh->indices.size());
        return {};
    }

    Result BuildOpaqueCommandsOperation::ensure_instance_buffer(
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
                backend.unload(buffer_it->second);

            auto buffer = Uuid {};
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::VERTEX,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Instance Transforms",
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
                backend.update_buffer(buffer_it->second, transforms.data(), data_size, 0U);
            !result)
            return result;

        out_buffer = buffer_it->second;
        return {};
    }

    Result BuildOpaqueCommandsOperation::ensure_fallback_pipeline(IGraphicsBackend& backend)
    {
        if (_fallback_pipeline.is_valid())
            return {};
        return backend.upload_pipeline(make_fallback_pipeline_desc(), _fallback_pipeline);
    }

    Result BuildOpaqueCommandsOperation::ensure_fallback_geometry_buffers(
        IGraphicsBackend& backend,
        const std::vector<float>& vertices,
        const std::vector<uint32>& indices)
    {
        if (vertices.empty() || indices.empty())
            return {};

        const uint64 vertex_size =
            static_cast<uint64>(vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(indices.size()) * static_cast<uint64>(sizeof(uint32));

        if (!_fallback_vertex_buffer.is_valid() || vertex_size > _fallback_vertex_buffer_size)
        {
            if (_fallback_vertex_buffer.is_valid())
                backend.unload(_fallback_vertex_buffer);

            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::VERTEX,
                        .size = vertex_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Fallback Geometry Vertices",
                    },
                    vertices.data(),
                    vertex_size,
                    _fallback_vertex_buffer);
                !result)
                return result;

            _fallback_vertex_buffer_size = vertex_size;
        }
        else if (const auto result =
                     backend
                         .update_buffer(_fallback_vertex_buffer, vertices.data(), vertex_size, 0U);
                 !result)
        {
            return result;
        }

        if (!_fallback_index_buffer.is_valid() || index_size > _fallback_index_buffer_size)
        {
            if (_fallback_index_buffer.is_valid())
                backend.unload(_fallback_index_buffer);

            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::INDEX,
                        .size = index_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Fallback Geometry Indices",
                    },
                    indices.data(),
                    index_size,
                    _fallback_index_buffer);
                !result)
                return result;

            _fallback_index_buffer_size = index_size;
        }
        else if (const auto result =
                     backend.update_buffer(_fallback_index_buffer, indices.data(), index_size, 0U);
                 !result)
        {
            return result;
        }

        return {};
    }

    static std::vector<GraphicsResourceBinding> make_scene_uniform_bindings(
        const Uuid view_uniform_buffer,
        const Uuid material_uniform_buffer)
    {
        return std::vector<GraphicsResourceBinding> {
            GraphicsResourceBinding {.slot = 0U, .resource = view_uniform_buffer},
            GraphicsResourceBinding {.slot = 1U, .resource = material_uniform_buffer},
        };
    }

    Result BuildOpaqueCommandsOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data;
        render_data.opaque_commands.clear();
        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "BuildDirectionalShadowCommandsOperation requires IGraphicsBackend service.");
        }
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();
        const RenderData& scene_data = render_data;
        const Uuid view_uniform_buffer = frame_data.view_uniform_buffer;

        // -------------------------------------------------------------------
        // Load material draw state — uses the shared ensure_material_uniform_buffer.
        // -------------------------------------------------------------------
        const auto load_and_store_material =
            [&](const MaterialInstance& material,
                Uuid& out_pipeline,
                uint64& out_material_key,
                Uuid& out_uniform_buffer,
                std::vector<GraphicsResourceBinding>& out_textures) -> Result
        {
            auto material_resource = GraphicsMaterialDrawResource {};
            if (const auto result =
                    resource_manager.load_material_draw_resource(material, material_resource);
                !result)
                return result;

            Uuid uniform_buffer = {};
            if (const auto result = ensure_material_uniform_buffer(
                    backend,
                    material_resource.uniform_key,
                    material_resource.uniform_data.data(),
                    material_resource.uniform_data.byte_size(),
                    uniform_buffer);
                !result)
                return result;

            out_pipeline = material_resource.pipeline;
            out_material_key = material_resource.uniform_key;
            out_uniform_buffer = uniform_buffer;
            out_textures = std::move(material_resource.textures);
            return {};
        };

        // -------------------------------------------------------------------
        // Dynamic mesh pass — batch by (mesh × material)
        // -------------------------------------------------------------------

        auto fallback_geometry = GeometryBuildResult {};
        auto dynamic_batches = std::unordered_map<uint64, DynamicBatch> {};
        auto prepare_result = Result {};

        for (const auto& renderable : scene_data.renderables)
        {
            if (!prepare_result)
                break;
            if (!renderable.is_visible
                || renderable.geometry_source != RenderDataGeometrySource::DynamicMesh)
                continue;

            const std::shared_ptr<Mesh>& mesh_data = renderable.dynamic_mesh;
            if (!mesh_data)
                continue;

            const Mat4 model_to_world = build_transform_matrix(renderable.transform);

            if (!can_render_mesh_directly(*mesh_data))
            {
                append_mesh_geometry(
                    *mesh_data,
                    frame_data.view_projection * model_to_world,
                    fallback_geometry);
                continue;
            }

            Uuid pipeline = {};
            uint64 material_key = 0U;
            Uuid uniform_buffer = {};
            auto textures = std::vector<GraphicsResourceBinding> {};
            prepare_result = load_and_store_material(
                renderable.material,
                pipeline,
                material_key,
                uniform_buffer,
                textures);
            if (!prepare_result)
                break;

            const uint64 mesh_key = make_mesh_cache_key(mesh_data);
            const uint64 batch_key = make_dynamic_batch_key(mesh_key, material_key);
            auto& batch = dynamic_batches[batch_key];
            if (!batch.mesh_data)
            {
                batch.mesh_data = mesh_data;
                batch.pipeline = pipeline;
                batch.material_key = material_key;
                batch.material_uniform_buffer = uniform_buffer;
                batch.textures = std::move(textures);
            }
            batch.transforms.push_back(model_to_world);
        }

        if (!prepare_result)
            return prepare_result;

        // -------------------------------------------------------------------
        // Static model pass — batch by (mesh × material)
        // -------------------------------------------------------------------

        auto static_batches = std::unordered_map<uint64, StaticBatch> {};

        for (const auto& renderable : scene_data.renderables)
        {
            if (!prepare_result)
                break;
            if (!renderable.is_visible
                || renderable.geometry_source != RenderDataGeometrySource::StaticMesh)
                continue;
            if (!renderable.static_mesh.is_valid())
                continue;

            auto model_resource = GraphicsModelResource {};
            prepare_result = resource_manager.load_model(renderable.static_mesh, model_resource);
            if (!prepare_result)
                break;

            const Mat4 model_to_world = build_transform_matrix(renderable.transform);

            Uuid pipeline = {};
            uint64 material_key = 0U;
            Uuid uniform_buffer = {};
            auto textures = std::vector<GraphicsResourceBinding> {};
            prepare_result = load_and_store_material(
                renderable.material,
                pipeline,
                material_key,
                uniform_buffer,
                textures);
            if (!prepare_result)
                break;

            for (const auto& mesh : model_resource.meshes)
            {
                const uint64 batch_key = make_static_batch_key(
                    mesh.vertex_buffer,
                    mesh.index_buffer,
                    mesh.index_count,
                    material_key);
                auto& batch = static_batches[batch_key];
                if (!batch.vertex_buffer.is_valid())
                {
                    batch.vertex_buffer = mesh.vertex_buffer;
                    batch.index_buffer = mesh.index_buffer;
                    batch.index_count = mesh.index_count;
                    batch.pipeline = pipeline;
                    batch.material_key = material_key;
                    batch.material_uniform_buffer = uniform_buffer;
                    batch.textures = textures;
                }
                batch.transforms.push_back(model_to_world);
            }
        }

        if (!prepare_result)
            return prepare_result;

        // -------------------------------------------------------------------
        // Upload/update instance buffers and build draw commands
        // -------------------------------------------------------------------

        for (auto& [batch_key, batch] : dynamic_batches)
        {
            if (!batch.mesh_data || batch.transforms.empty())
                continue;

            Uuid vertex_buffer = {};
            Uuid index_buffer = {};
            uint32 index_count = 0U;
            if (const auto result = ensure_dynamic_mesh_buffers(
                    backend,
                    batch.mesh_data,
                    vertex_buffer,
                    index_buffer,
                    index_count);
                !result)
                return result;

            Uuid instance_buffer = {};
            if (const auto result =
                    ensure_instance_buffer(backend, batch_key, batch.transforms, instance_buffer);
                !result)
                return result;

            auto textures = std::move(batch.textures);

            render_data.opaque_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = vertex_buffer},
                            GraphicsResourceBinding {.slot = 1U, .resource = instance_buffer},
                        },
                    .index_buffer = index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers = make_scene_uniform_bindings(
                        view_uniform_buffer,
                        batch.material_uniform_buffer),
                    .textures = std::move(textures),
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count = static_cast<uint32>(batch.transforms.size()),
                        },
                });
        }

        for (auto& [batch_key, batch] : static_batches)
        {
            if (!batch.vertex_buffer.is_valid() || batch.transforms.empty())
                continue;

            Uuid instance_buffer = {};
            if (const auto result =
                    ensure_instance_buffer(backend, batch_key, batch.transforms, instance_buffer);
                !result)
                return result;

            auto textures = std::move(batch.textures);

            render_data.opaque_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = batch.vertex_buffer},
                            GraphicsResourceBinding {.slot = 1U, .resource = instance_buffer},
                        },
                    .index_buffer = batch.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers = make_scene_uniform_bindings(
                        view_uniform_buffer,
                        batch.material_uniform_buffer),
                    .textures = std::move(textures),
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = batch.index_count,
                            .instance_count = static_cast<uint32>(batch.transforms.size()),
                        },
                });
        }

        // -------------------------------------------------------------------
        // Fallback geometry — CPU-clipped meshes
        // -------------------------------------------------------------------

        if (!fallback_geometry.vertices.empty() && !fallback_geometry.indices.empty())
        {
            if (const auto result = ensure_fallback_pipeline(backend); !result)
                return result;

            if (const auto result = ensure_fallback_geometry_buffers(
                    backend,
                    fallback_geometry.vertices,
                    fallback_geometry.indices);
                !result)
                return result;

            render_data.opaque_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = _fallback_pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = _fallback_vertex_buffer},
                        },
                    .index_buffer = _fallback_index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = static_cast<uint32>(fallback_geometry.indices.size()),
                            .instance_count = 1U,
                        },
                });
        }

        return {};
    }

    Result BuildOpaqueCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildOpaqueCommandsOperation::release(IGraphicsBackend& backend)
    {
        for (const auto& [key, uuid] : _mesh_vertex_buffers)
            backend.unload(uuid);
        for (const auto& [key, uuid] : _mesh_index_buffers)
            backend.unload(uuid);
        for (const auto& [key, uuid] : _instance_buffers)
            backend.unload(uuid);
        for (const auto& [key, uuid] : _material_uniform_buffers)
            backend.unload(uuid);

        if (_fallback_pipeline.is_valid())
            backend.unload(_fallback_pipeline);
        if (_fallback_vertex_buffer.is_valid())
            backend.unload(_fallback_vertex_buffer);
        if (_fallback_index_buffer.is_valid())
            backend.unload(_fallback_index_buffer);

        _mesh_vertex_buffers.clear();
        _mesh_index_buffers.clear();
        _mesh_index_counts.clear();
        _mesh_sources.clear();
        _instance_buffers.clear();
        _instance_buffer_sizes.clear();
        _material_uniform_buffers.clear();
        _fallback_pipeline = {};
        _fallback_vertex_buffer = {};
        _fallback_index_buffer = {};
        _fallback_vertex_buffer_size = 0U;
        _fallback_index_buffer_size = 0U;
    }

    static RenderOperationDebugInfo make_command_debug_info(
        const std::string& name,
        const std::string& category)
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = name;
        debug_info.category = category;
        return debug_info;
    }

    RenderOperationDebugInfo BuildAlphaCutoutCommandsOperation::get_debug_info() const
    {
        return make_command_debug_info(
            "Toybox Build Alpha Cutout Commands Operation",
            "Command Building");
    }

    Result BuildAlphaCutoutCommandsOperation::prepare(RenderData& render_data)
    {
        render_data.alpha_cutout_commands.clear();
        return {};
    }

    Result BuildAlphaCutoutCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    RenderOperationDebugInfo BuildTransparentCommandsOperation::get_debug_info() const
    {
        return make_command_debug_info(
            "Toybox Build Transparent Commands Operation",
            "Command Building");
    }

    Result BuildTransparentCommandsOperation::prepare(RenderData& render_data)
    {
        render_data.transparent_commands.clear();
        return {};
    }

    Result BuildTransparentCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    BuildFullscreenQuadResourcesOperation::BuildFullscreenQuadResourcesOperation(
        std::weak_ptr<IGraphicsBackend> backend)
        : _backend(std::move(backend))
    {
    }

    RenderOperationDebugInfo BuildFullscreenQuadResourcesOperation::get_debug_info() const
    {
        return make_command_debug_info(
            "Toybox Build Fullscreen Quad Resources Operation",
            "Command Building");
    }

    Result BuildFullscreenQuadResourcesOperation::ensure_geometry(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid() && _index_buffer.is_valid() && _index_count > 0U)
            return {};

        const Mesh& mesh = fullscreen_quad;
        const uint64 vertex_size =
            static_cast<uint64>(mesh.vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_size =
            static_cast<uint64>(mesh.indices.size()) * static_cast<uint64>(sizeof(uint32));

        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Fullscreen Quad Vertices",
                },
                mesh.vertices.data(),
                vertex_size,
                _vertex_buffer);
            !result)
        {
            return result;
        }

        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Fullscreen Quad Indices",
                },
                mesh.indices.data(),
                index_size,
                _index_buffer);
            !result)
        {
            backend.unload(_vertex_buffer);
            _vertex_buffer = {};
            return result;
        }

        _index_count = static_cast<uint32>(mesh.indices.size());
        return {};
    }

    Result BuildFullscreenQuadResourcesOperation::prepare(RenderData& render_data)
    {
        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "BuildFullscreenQuadResourcesOperation requires IGraphicsBackend service.");
        }

        if (const auto result = ensure_geometry(*backend_ptr); !result)
            return result;

        render_data.fullscreen_quad_vertex_buffer = _vertex_buffer;
        render_data.fullscreen_quad_index_buffer = _index_buffer;
        render_data.fullscreen_quad_index_count = _index_count;
        return {};
    }

    Result BuildFullscreenQuadResourcesOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildFullscreenQuadResourcesOperation::release(IGraphicsBackend& backend)
    {
        if (_vertex_buffer.is_valid())
            backend.unload(_vertex_buffer);
        if (_index_buffer.is_valid())
            backend.unload(_index_buffer);

        _vertex_buffer = {};
        _index_buffer = {};
        _index_count = 0U;
    }

    BuildLightingCommandsOperation::BuildLightingCommandsOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo BuildLightingCommandsOperation::get_debug_info() const
    {
        return make_command_debug_info(
            "Toybox Build Lighting Commands Operation",
            "Command Building");
    }

    uint32 BuildLightingCommandsOperation::get_albedo_texture_slot()
    {
        return 0U;
    }

    uint32 BuildLightingCommandsOperation::get_normal_texture_slot()
    {
        return 1U;
    }

    uint32 BuildLightingCommandsOperation::get_emissive_texture_slot()
    {
        return 2U;
    }

    uint32 BuildLightingCommandsOperation::get_material_texture_slot()
    {
        return 3U;
    }

    uint32 BuildLightingCommandsOperation::get_depth_texture_slot()
    {
        return 4U;
    }

    uint32 BuildLightingCommandsOperation::get_directional_shadow_texture_slot()
    {
        return 5U;
    }

    uint32 BuildLightingCommandsOperation::get_point_shadow_texture_slot()
    {
        return 6U;
    }

    uint32 BuildLightingCommandsOperation::get_spot_shadow_texture_slot()
    {
        return 7U;
    }

    uint32 BuildLightingCommandsOperation::get_area_shadow_texture_slot()
    {
        return 8U;
    }

    uint32 BuildLightingCommandsOperation::get_material_uniform_slot()
    {
        return 1U;
    }

    uint32 BuildLightingCommandsOperation::get_lighting_info_uniform_slot()
    {
        return 7U;
    }

    uint32 BuildLightingCommandsOperation::get_point_lights_storage_slot()
    {
        return 0U;
    }

    uint32 BuildLightingCommandsOperation::get_spot_lights_storage_slot()
    {
        return 1U;
    }

    uint32 BuildLightingCommandsOperation::get_tile_light_spans_storage_slot()
    {
        return 2U;
    }

    uint32 BuildLightingCommandsOperation::get_tile_point_light_indices_storage_slot()
    {
        return 3U;
    }

    uint32 BuildLightingCommandsOperation::get_tile_spot_light_indices_storage_slot()
    {
        return 4U;
    }

    uint32 BuildLightingCommandsOperation::get_area_lights_storage_slot()
    {
        return 5U;
    }

    uint32 BuildLightingCommandsOperation::get_tile_area_light_indices_storage_slot()
    {
        return 6U;
    }

    uint32 BuildLightingCommandsOperation::get_directional_shadow_cascades_storage_slot()
    {
        return 8U;
    }

    uint32 BuildLightingCommandsOperation::get_spot_shadow_maps_storage_slot()
    {
        return 10U;
    }

    uint32 BuildLightingCommandsOperation::get_area_shadow_maps_storage_slot()
    {
        return 11U;
    }

    static bool material_binding_name_matches(
        const std::string_view binding_name,
        const std::string_view canonical_name)
    {
        if (binding_name == canonical_name)
            return true;

        if (binding_name.size() == canonical_name.size() + 2U && binding_name[0] == 'u'
            && binding_name[1] == '_')
        {
            return binding_name.substr(2U) == canonical_name;
        }

        return false;
    }

    int32 BuildPostProcessCommandsOperation::find_parameter_index(
        const GraphicsMaterialDrawResource& resource,
        const char* parameter_name)
    {
        if (!parameter_name || !parameter_name[0])
            return -1;

        for (uint32 index = 0U; index < resource.parameter_names.size(); ++index)
        {
            if (material_binding_name_matches(
                    resource.parameter_names[index],
                    parameter_name))
            {
                return static_cast<int32>(index);
            }
        }

        return -1;
    }

    int32 BuildPostProcessCommandsOperation::find_texture_binding_slot(
        const GraphicsMaterialDrawResource& resource,
        const char* texture_name)
    {
        if (!texture_name || !texture_name[0])
            return -1;

        for (uint32 index = 0U; index < resource.texture_names.size(); ++index)
        {
            if (!material_binding_name_matches(resource.texture_names[index], texture_name))
                continue;

            if (index < resource.textures.size())
                return static_cast<int32>(resource.textures[index].slot);

            return static_cast<int32>(index);
        }

        return -1;
    }

    void BuildPostProcessCommandsOperation::apply_effect_blend_uniform(
        GraphicsMaterialDrawResource& resource,
        const float blend)
    {
        const int32 blend_parameter_index = find_parameter_index(resource, "blend");
        if (blend_parameter_index < 0)
            return;

        const uint32 uniform_index = static_cast<uint32>(blend_parameter_index);
        if (uniform_index >= resource.uniform_data.values.size())
            return;

        resource.uniform_data.values[uniform_index].x = blend;
    }

    void BuildLightingCommandsOperation::set_texture_binding(
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

    Result BuildLightingCommandsOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (data_size == 0U || !data)
            return Result(false, "BuildLightingCommandsOperation: invalid material uniform data.");

        const auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
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

    static Result upload_or_update_buffer(
        IGraphicsBackend& backend,
        const GraphicsBufferUsage usage,
        const std::string& debug_name,
        const void* data,
        const uint64 data_size,
        Uuid& buffer,
        uint64& capacity)
    {
        if (data == nullptr || data_size == 0U)
            return Result(false, "BuildLightingCommandsOperation: invalid buffer upload data.");

        if (!buffer.is_valid() || data_size > capacity)
        {
            if (buffer.is_valid())
                backend.unload(buffer);

            auto created = Uuid {};
            if (const auto result = backend.upload_buffer(
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

        return backend.update_buffer(buffer, data, data_size, 0U);
    }

    Result BuildLightingCommandsOperation::ensure_lighting_buffers(
        IGraphicsBackend& backend,
        const RenderData& render_data)
    {
        const auto find_spot_shadow_index =
            [&render_data](const Uuid entity_uuid) -> int32
        {
            for (uint32 index = 0U; index < render_data.spot_shadow_maps.size(); ++index)
            {
                if (render_data.spot_shadow_maps[index].entity_uuid == entity_uuid)
                    return static_cast<int32>(index);
            }
            return -1;
        };
        const auto find_area_shadow_index =
            [&render_data](const Uuid entity_uuid) -> int32
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
                    .radiance_shadow_bias =
                        Vec4(Vec3(make_light_radiance(light.light)), 0.00035F),
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
                    .direction_inner_cos =
                        Vec4(make_light_direction(light.transform), inner_cos),
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
                    .radiance_half_height =
                        Vec4(Vec3(radiance), light.light.area_size.y * 0.5F),
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
            const bool has_shadows = light.entity_uuid == render_data.directional_shadow_light_entity
                                     && !render_data.directional_shadow_cascades.empty();
            lighting_info.directional_lights[index] = DirectionalLightGpu {
                .direction_ambient = Vec4(make_light_direction(light.transform), light.light.ambient),
                .radiance_shadowed =
                    Vec4(Vec3(make_light_radiance(light.light)), has_shadows ? 1.0F : 0.0F),
                .shadow_info =
                    IVec4(0, has_shadows
                                  ? static_cast<int32>(render_data.directional_shadow_cascades.size())
                                  : 0,
                          0,
                          0),
            };
        }

        const uint32 tile_size = 16U;
        const uint32 tile_count_x = std::max(1U, (render_data.viewport.dimensions.width + tile_size - 1U) / tile_size);
        const uint32 tile_count_y =
            std::max(1U, (render_data.viewport.dimensions.height + tile_size - 1U) / tile_size);
        lighting_info.tile_info = IVec4(
            static_cast<int32>(tile_size),
            static_cast<int32>(tile_count_x),
            static_cast<int32>(tile_count_y),
            0);

        auto tile_light_spans =
            std::vector<TileLightSpanGpu>(tile_count_x * tile_count_y);
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
        for (uint32 cascade_index = 0U; cascade_index < render_data.directional_shadow_cascades.size();
             ++cascade_index)
        {
            const auto& cascade = render_data.directional_shadow_cascades[cascade_index];
            directional_shadow_cascades.push_back(
                ShadowCascadeGpu {
                    .light_view_projection = cascade.light_view_projection,
                    .split_and_bias =
                        Vec4(cascade.split_depth, cascade.normal_bias, cascade.depth_bias, cascade.blend_distance),
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
                    .planes_and_bias =
                        Vec4(shadow_map.z_near, shadow_map.z_far, shadow_map.normal_bias, shadow_map.depth_bias),
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
                    .planes_and_bias =
                        Vec4(shadow_map.z_near, shadow_map.z_far, shadow_map.normal_bias, shadow_map.depth_bias),
                    .texture_layer = IVec4(static_cast<int32>(shadow_map.texture_layer), 0, 0, 0),
                });
        }

        const auto upload_vector = [&backend](
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
            const uint64 bytes = has_values ? static_cast<uint64>(values.size()) * static_cast<uint64>(sizeof(TValue))
                                            : static_cast<uint64>(sizeof(TValue));
            return upload_or_update_buffer(backend, usage, name, data, bytes, buffer, size_bytes);
        };

        if (const auto result = upload_or_update_buffer(
                backend,
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

    Result BuildPostProcessCommandsOperation::ensure_post_process_targets(
        IGraphicsBackend& backend,
        const Size& resolution)
    {
        if (resolution.width == 0U || resolution.height == 0U)
        {
            return Result(
                false,
                "BuildPostProcessCommandsOperation: post-process resolution is invalid.");
        }

        const bool matches_resolution = _target_resolution.width == resolution.width
                                        && _target_resolution.height == resolution.height;
        const bool has_all_targets = std::all_of(
            _post_process_targets.begin(),
            _post_process_targets.end(),
            [](const Uuid target)
            {
                return target.is_valid();
            });
        if (matches_resolution && has_all_targets)
            return {};

        for (auto& target : _post_process_targets)
        {
            if (target.is_valid())
                backend.unload(target);
            target = {};
        }
        _target_resolution = {};

        const auto target_names = std::array<std::string, 2U> {
            "Toybox Post Process Ping Target",
            "Toybox Post Process Pong Target",
        };
        for (uint32 index = 0U; index < _post_process_targets.size(); ++index)
        {
            if (const auto result = backend.upload_texture(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_RENDER_TARGET,
                        .format = GraphicsTextureFormat::RGBA16_FLOAT,
                        .size = resolution,
                        .mip_count = 1U,
                        .array_layer_count = 1U,
                        .debug_name = target_names[index],
                    },
                    nullptr,
                    0U,
                    _post_process_targets[index]);
                !result)
            {
                for (auto& target : _post_process_targets)
                {
                    if (target.is_valid())
                        backend.unload(target);
                    target = {};
                }
                return result;
            }
        }

        _target_resolution = resolution;
        return {};
    }

    Result BuildLightingCommandsOperation::ensure_render_targets(
        IGraphicsBackend& backend,
        const Size& resolution)
    {
        if (resolution.width == 0U || resolution.height == 0U)
            return Result(false, "BuildLightingCommandsOperation: render resolution is invalid.");

        const bool matches_resolution = _target_resolution.width == resolution.width
                                        && _target_resolution.height == resolution.height;
        const bool has_all_color_targets = std::all_of(
            _scene_color_targets.begin(),
            _scene_color_targets.end(),
            [](const Uuid target)
            {
                return target.is_valid();
            });
        if (matches_resolution && has_all_color_targets && _scene_depth_target.is_valid())
        {
            return {};
        }

        for (auto& color_target : _scene_color_targets)
        {
            if (color_target.is_valid())
                backend.unload(color_target);
            color_target = {};
        }
        if (_scene_depth_target.is_valid())
            backend.unload(_scene_depth_target);

        _scene_depth_target = {};
        _target_resolution = {};

        const auto color_target_names = std::array<std::string, 7U> {
            "Toybox Scene Color Target",
            "Toybox Scene World Position Target",
            "Toybox Scene Albedo Target",
            "Toybox Scene Normal Target",
            "Toybox Scene Depth Preview Target",
            "Toybox Scene Emissive Target",
            "Toybox Scene Material Target",
        };
        for (uint32 index = 0U; index < color_target_names.size(); ++index)
        {
            auto& color_target = _scene_color_targets[index];
            if (const auto result = backend.upload_texture(
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

        if (const auto result = backend.upload_texture(
                GraphicsTextureDesc {
                    .usage = GraphicsTextureUsage::DEPTH_STENCIL,
                    .format = GraphicsTextureFormat::DEPTH24_STENCIL8,
                    .size = resolution,
                    .mip_count = 1U,
                    .array_layer_count = 1U,
                    .debug_name = "Toybox Scene Depth Target",
                },
                nullptr,
                0U,
                _scene_depth_target);
            !result)
        {
            for (auto& color_target : _scene_color_targets)
            {
                if (color_target.is_valid())
                    backend.unload(color_target);
                color_target = {};
            }
            return result;
        }

        _target_resolution = resolution;
        return {};
    }

    Result BuildLightingCommandsOperation::prepare(RenderData& render_data)
    {
        render_data.scene_color_targets.clear();
        render_data.scene_color_target = {};
        render_data.scene_world_position_target = {};
        render_data.scene_albedo_target = {};
        render_data.scene_normal_target = {};
        render_data.scene_emissive_target = {};
        render_data.scene_material_target = {};
        render_data.scene_depth_target = {};
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
                "BuildLightingCommandsOperation requires IGraphicsBackend service.");
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
        if (const auto result = resource_manager.load_post_process_material_draw_resource(
                lighting_material,
                lighting_resource);
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

        render_data.scene_color_targets = std::vector<Uuid> {
            _scene_color_targets[0U],
            _scene_color_targets[1U],
            _scene_color_targets[2U],
            _scene_color_targets[3U],
            _scene_color_targets[4U],
            _scene_color_targets[5U],
            _scene_color_targets[6U],
        };
        render_data.scene_color_target = _scene_color_targets[0U];
        render_data.scene_world_position_target = _scene_color_targets[1U];
        render_data.scene_albedo_target = _scene_color_targets[2U];
        render_data.scene_normal_target = _scene_color_targets[3U];
        render_data.scene_emissive_target = _scene_color_targets[5U];
        render_data.scene_material_target = _scene_color_targets[6U];
        render_data.scene_depth_target = _scene_depth_target;

        auto lighting_textures = std::move(lighting_resource.textures);
        set_texture_binding(
            lighting_textures,
            get_albedo_texture_slot(),
            render_data.scene_albedo_target);
        set_texture_binding(
            lighting_textures,
            get_normal_texture_slot(),
            render_data.scene_normal_target);
        set_texture_binding(
            lighting_textures,
            get_emissive_texture_slot(),
            render_data.scene_emissive_target);
        set_texture_binding(
            lighting_textures,
            get_material_texture_slot(),
            render_data.scene_material_target);
        set_texture_binding(
            lighting_textures,
            get_depth_texture_slot(),
            render_data.scene_depth_target);
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
            lighting_pass_desc.color_targets = {render_data.scene_color_target};

        if (!render_data.fullscreen_quad_vertex_buffer.is_valid()
            || !render_data.fullscreen_quad_index_buffer.is_valid()
            || render_data.fullscreen_quad_index_count == 0U)
        {
            return Result(
                false,
                "BuildLightingCommandsOperation: fullscreen quad resources are unavailable.");
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

    Result BuildLightingCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildLightingCommandsOperation::release(IGraphicsBackend& backend)
    {
        for (const auto& [key, uuid] : _material_uniform_buffers)
            backend.unload(uuid);

        for (auto& color_target : _scene_color_targets)
        {
            if (color_target.is_valid())
                backend.unload(color_target);
            color_target = {};
        }
        if (_scene_depth_target.is_valid())
            backend.unload(_scene_depth_target);
        if (_lighting_info_uniform_buffer.is_valid())
            backend.unload(_lighting_info_uniform_buffer);
        if (_point_lights_storage_buffer.is_valid())
            backend.unload(_point_lights_storage_buffer);
        if (_spot_lights_storage_buffer.is_valid())
            backend.unload(_spot_lights_storage_buffer);
        if (_tile_light_spans_storage_buffer.is_valid())
            backend.unload(_tile_light_spans_storage_buffer);
        if (_tile_point_light_indices_storage_buffer.is_valid())
            backend.unload(_tile_point_light_indices_storage_buffer);
        if (_tile_spot_light_indices_storage_buffer.is_valid())
            backend.unload(_tile_spot_light_indices_storage_buffer);
        if (_area_lights_storage_buffer.is_valid())
            backend.unload(_area_lights_storage_buffer);
        if (_tile_area_light_indices_storage_buffer.is_valid())
            backend.unload(_tile_area_light_indices_storage_buffer);
        if (_directional_shadow_cascades_storage_buffer.is_valid())
            backend.unload(_directional_shadow_cascades_storage_buffer);
        if (_spot_shadow_maps_storage_buffer.is_valid())
            backend.unload(_spot_shadow_maps_storage_buffer);
        if (_area_shadow_maps_storage_buffer.is_valid())
            backend.unload(_area_shadow_maps_storage_buffer);

        _material_uniform_buffers.clear();
        _scene_depth_target = {};
        _lighting_info_uniform_buffer = {};
        _point_lights_storage_buffer = {};
        _spot_lights_storage_buffer = {};
        _tile_light_spans_storage_buffer = {};
        _tile_point_light_indices_storage_buffer = {};
        _tile_spot_light_indices_storage_buffer = {};
        _area_lights_storage_buffer = {};
        _tile_area_light_indices_storage_buffer = {};
        _directional_shadow_cascades_storage_buffer = {};
        _spot_shadow_maps_storage_buffer = {};
        _area_shadow_maps_storage_buffer = {};
        _lighting_info_uniform_buffer_size = 0U;
        _point_lights_storage_buffer_size = 0U;
        _spot_lights_storage_buffer_size = 0U;
        _tile_light_spans_storage_buffer_size = 0U;
        _tile_point_light_indices_storage_buffer_size = 0U;
        _tile_spot_light_indices_storage_buffer_size = 0U;
        _area_lights_storage_buffer_size = 0U;
        _tile_area_light_indices_storage_buffer_size = 0U;
        _directional_shadow_cascades_storage_buffer_size = 0U;
        _spot_shadow_maps_storage_buffer_size = 0U;
        _area_shadow_maps_storage_buffer_size = 0U;
        _target_resolution = {};
    }

    BuildPostProcessCommandsOperation::BuildPostProcessCommandsOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo BuildPostProcessCommandsOperation::get_debug_info() const
    {
        return make_command_debug_info(
            "Toybox Build Post Process Commands Operation",
            "Command Building");
    }

    void BuildPostProcessCommandsOperation::set_texture_binding(
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

    Result BuildPostProcessCommandsOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (data_size == 0U || data == nullptr)
        {
            return Result(
                false,
                "BuildPostProcessCommandsOperation: invalid material uniform data.");
        }

        const auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Post Process Material Uniforms",
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

    Result BuildPostProcessCommandsOperation::prepare(RenderData& render_data)
    {
        render_data.post_process_passes.clear();

        if (!render_data.post_processing.is_enabled)
            return {};
        if (!render_data.scene_color_target.is_valid())
            return {};

        auto enabled_effects = std::vector<const PostProcessingEffect*> {};
        enabled_effects.reserve(render_data.post_processing.effects.size());
        for (const auto& effect : render_data.post_processing.effects)
        {
            if (!effect.is_enabled || !effect.material.get_handle().is_valid())
                continue;
            enabled_effects.push_back(&effect);
        }
        if (enabled_effects.empty())
            return {};

        if (!render_data.fullscreen_quad_vertex_buffer.is_valid()
            || !render_data.fullscreen_quad_index_buffer.is_valid()
            || render_data.fullscreen_quad_index_count == 0U)
        {
            return Result(
                false,
                "BuildPostProcessCommandsOperation: fullscreen quad resources are unavailable.");
        }

        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "BuildPostProcessCommandsOperation requires IGraphicsBackend service.");
        }
        auto& backend = *backend_ptr;
        auto& resource_manager = _resource_manager.get();

        if (enabled_effects.size() > 1U)
        {
            if (const auto result =
                    ensure_post_process_targets(backend, render_data.render_resolution);
                !result)
            {
                return result;
            }
        }

        auto source_texture = render_data.scene_color_target;
        auto output_target_index = uint32 {0U};
        for (uint32 effect_index = 0U; effect_index < enabled_effects.size(); ++effect_index)
        {
            const PostProcessingEffect& effect = *enabled_effects[effect_index];
            const bool is_last_effect = effect_index + 1U == enabled_effects.size();

            auto effect_resource = GraphicsMaterialDrawResource {};
            if (const auto result = resource_manager.load_post_process_material_draw_resource(
                    effect.material,
                    effect_resource);
                !result)
            {
                return result;
            }

            apply_effect_blend_uniform(effect_resource, effect.blend);
            const int32 scene_color_slot =
                find_texture_binding_slot(effect_resource, "scene_color");
            if (scene_color_slot < 0)
            {
                return Result(
                    false,
                    "BuildPostProcessCommandsOperation: post-process material '"
                        + to_string(effect.material.get_handle())
                        + "' is missing a 'scene_color' texture binding.");
            }

            auto effect_textures = std::move(effect_resource.textures);
            set_texture_binding(
                effect_textures,
                static_cast<uint32>(scene_color_slot),
                source_texture);

            auto effect_material_uniform_buffer = Uuid {};
            if (const auto result = ensure_material_uniform_buffer(
                    backend,
                    effect_resource.uniform_key,
                    effect_resource.uniform_data.data(),
                    effect_resource.uniform_data.byte_size(),
                    effect_material_uniform_buffer);
                !result)
            {
                return result;
            }

            auto pass_desc = GraphicsPassDesc {
                .clear_color = Color::BLACK,
                .clear_depth = 1.0F,
                .clear_flags = is_last_effect ? GraphicsClearFlags::NONE
                                              : GraphicsClearFlags::COLOR,
                .debug_name = "Toybox Post Process Pass",
            };
            if (!is_last_effect)
            {
                pass_desc.color_targets = {_post_process_targets[output_target_index]};
            }

            render_data.post_process_passes.push_back(
                GraphicsRenderPass {
                    .pass = std::move(pass_desc),
                    .viewport = render_data.viewport,
                    .indexed_draws =
                        {
                            GraphicsIndexedDrawCommand {
                                .pipeline = effect_resource.pipeline,
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
                                            .slot = 1U,
                                            .resource = effect_material_uniform_buffer},
                                    },
                                .textures = std::move(effect_textures),
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

            if (!is_last_effect)
            {
                source_texture = _post_process_targets[output_target_index];
                output_target_index = output_target_index == 0U ? 1U : 0U;
            }
        }

        return {};
    }

    Result BuildPostProcessCommandsOperation::execute(
        IGraphicsBackend&,
        RenderData&,
        const CancellationToken&)
    {
        return {};
    }

    void BuildPostProcessCommandsOperation::release(IGraphicsBackend& backend)
    {
        for (const auto& [material_key, uniform_buffer] : _material_uniform_buffers)
        {
            (void)material_key;
            backend.unload(uniform_buffer);
        }
        _material_uniform_buffers.clear();

        for (auto& target : _post_process_targets)
        {
            if (target.is_valid())
                backend.unload(target);
            target = {};
        }
        _target_resolution = {};
    }
}




