#include "tbx/systems/graphics/pipeline/commands/command_building_operations.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/pipeline/commands/render_command_executor.h"
#include "tbx/systems/graphics/pipeline/context/render_data.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/types/components/camera.h"
#include "tbx/types/components/mesh.h"
#include "tbx/types/material.h"
#include "tbx/types/matrices.h"
#include "tbx/types/shader.h"
#include "tbx/utils/hash.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace tbx
{
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
        auto& frame_data = render_data.frame;
        render_data.skybox_commands.clear();
        render_data.has_skybox = false;

        const RenderData& scene_data = render_data;
        if (!scene_data.sky.sky.material.get_handle().is_valid())
            return {};

        const MaterialInstance& sky_material = scene_data.sky.sky.material;
        const Transform& sky_transform = scene_data.sky.transform;

        auto& backend = frame_data.backend.get();
        auto& resource_manager = frame_data.resource_manager.get();

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

    inline constexpr uint32 TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS = 4U;
    inline constexpr uint32 TBX_MAX_FORWARD_POINT_LIGHTS = 16U;
    inline constexpr uint32 TBX_MAX_FORWARD_SPOT_LIGHTS = 8U;
    inline constexpr uint32 TBX_MAX_FORWARD_AREA_LIGHTS = 4U;

    struct ForwardDirectionalLight
    {
        Vec4 direction_ambient = Vec4(0.0F);
        Vec4 radiance = Vec4(0.0F);
        IVec4 shadow_info = IVec4(0);
    };

    struct ForwardPointLight
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 radiance = Vec4(0.0F);
    };

    struct ForwardSpotLight
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 direction_inner_cos = Vec4(0.0F);
        Vec4 radiance_outer_cos = Vec4(0.0F);
    };

    struct ForwardAreaLight
    {
        Vec4 position_range = Vec4(0.0F);
        Vec4 direction_half_width = Vec4(0.0F);
        Vec4 radiance_half_height = Vec4(0.0F);
        Vec4 right = Vec4(0.0F);
        Vec4 up = Vec4(0.0F);
    };

    struct ForwardLightingUniformBlock
    {
        Vec4 camera_position = Vec4(0.0F);
        IVec4 light_counts = IVec4(0);
        ForwardDirectionalLight directional_lights[TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS] = {};
        ForwardPointLight point_lights[TBX_MAX_FORWARD_POINT_LIGHTS] = {};
        ForwardSpotLight spot_lights[TBX_MAX_FORWARD_SPOT_LIGHTS] = {};
        ForwardAreaLight area_lights[TBX_MAX_FORWARD_AREA_LIGHTS] = {};
    };

    inline constexpr uint32 TBX_FORWARD_SHADOW_CASCADE_COUNT = 3U;
    inline constexpr uint32 TBX_FORWARD_SHADOW_TEXTURE_BINDING = 8U;

    struct ForwardShadowCascade
    {
        Mat4 light_view_projection = Mat4(1.0F);
        Vec4 split_bias_blend = Vec4(0.0F);
    };

    struct ForwardShadowUniformBlock
    {
        IVec4 shadow_counts = IVec4(0);
        Vec4 shadow_settings = Vec4(0.0F);
        Vec4 camera_forward = Vec4(0.0F);
        ForwardShadowCascade directional_cascades[TBX_FORWARD_SHADOW_CASCADE_COUNT] = {};
    };

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

    static ForwardLightingUniformBlock make_forward_lighting_uniforms(const RenderData& render_data)
    {
        auto uniforms = ForwardLightingUniformBlock {
            .camera_position = Vec4(render_data.frame.camera_position, 1.0F),
        };

        const uint32 directional_count = std::min(
            static_cast<uint32>(render_data.directional_lights.size()),
            TBX_MAX_FORWARD_DIRECTIONAL_LIGHTS);
        const uint32 point_count = std::min(
            static_cast<uint32>(render_data.point_lights.size()),
            TBX_MAX_FORWARD_POINT_LIGHTS);
        const uint32 spot_count = std::min(
            static_cast<uint32>(render_data.spot_lights.size()),
            TBX_MAX_FORWARD_SPOT_LIGHTS);
        const uint32 area_count = std::min(
            static_cast<uint32>(render_data.area_lights.size()),
            TBX_MAX_FORWARD_AREA_LIGHTS);

        uniforms.light_counts =
            IVec4(directional_count, point_count, spot_count, area_count);

        for (uint32 index = 0U; index < directional_count; ++index)
        {
            const auto& light = render_data.directional_lights[index];
            uniforms.directional_lights[index] = ForwardDirectionalLight {
                .direction_ambient =
                    Vec4(make_light_direction(light.transform), light.light.ambient),
                .radiance = make_light_radiance(light.light),
                .shadow_info = IVec4(
                    0,
                    index == 0U
                        ? static_cast<int>(render_data.directional_shadow_cascades.size())
                        : 0,
                    0,
                    0),
            };
        }

        for (uint32 index = 0U; index < point_count; ++index)
        {
            const auto& light = render_data.point_lights[index];
            uniforms.point_lights[index] = ForwardPointLight {
                .position_range = Vec4(light.transform.position, light.light.range),
                .radiance = make_light_radiance(light.light),
            };
        }

        for (uint32 index = 0U; index < spot_count; ++index)
        {
            const auto& light = render_data.spot_lights[index];
            const float inner_cos = angle_to_cosine(light.light.inner_angle);
            const float outer_cos = angle_to_cosine(light.light.outer_angle);
            const Vec4 radiance = make_light_radiance(light.light);
            uniforms.spot_lights[index] = ForwardSpotLight {
                .position_range = Vec4(light.transform.position, light.light.range),
                .direction_inner_cos = Vec4(make_light_direction(light.transform), inner_cos),
                .radiance_outer_cos = Vec4(Vec3(radiance), outer_cos),
            };
        }

        for (uint32 index = 0U; index < area_count; ++index)
        {
            const auto& light = render_data.area_lights[index];
            const Vec3 direction = make_light_direction(light.transform);
            const Vec3 right = normalize(light.transform.rotation * Vec3(1.0F, 0.0F, 0.0F));
            const Vec3 up = normalize(light.transform.rotation * Vec3(0.0F, 1.0F, 0.0F));
            const Vec4 radiance = make_light_radiance(light.light);
            uniforms.area_lights[index] = ForwardAreaLight {
                .position_range = Vec4(light.transform.position, light.light.range),
                .direction_half_width = Vec4(direction, light.light.area_size.x * 0.5F),
                .radiance_half_height =
                    Vec4(Vec3(radiance), light.light.area_size.y * 0.5F),
                .right = Vec4(right, 0.0F),
                .up = Vec4(up, 0.0F),
            };
        }

        return uniforms;
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
            .debug_name = "Toybox Directional Shadow Pipeline",
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
        const FrameData& frame_data,
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
        const FrameData& frame_data,
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
            center -= normalize(light.transform.rotation * Vec3(1.0F, 0.0F, 0.0F))
                      * (std::fmod(light_center.x, texel_size));
            center -= normalize(light.transform.rotation * Vec3(0.0F, 1.0F, 0.0F))
                      * (std::fmod(light_center.y, texel_size));
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

        const float depth_margin = std::max(16.0F, radius);
        const float near_plane = std::max(0.1F, -maximum_z - depth_margin);
        const float far_plane = std::max(near_plane + 1.0F, -minimum_z + depth_margin);
        const Mat4 light_projection =
            ortho_projection(-radius, radius, -radius, radius, near_plane, far_plane);

        const float cascade_span = std::max(split_far - previous_split_far, 0.001F);
        return RenderDataDirectionalShadowCascade {
            .light_view_projection = light_projection * light_view,
            .split_depth = split_far,
            .normal_bias = std::clamp(radius * 0.0008F, 0.015F, 0.12F),
            .depth_bias = std::clamp(radius * 0.00004F, 0.0008F, 0.006F),
            .blend_distance = cascade_span * 0.12F,
            .texture = texture,
        };
    }

    static std::vector<float> make_cascade_splits(const FrameData& frame_data)
    {
        const float near_plane = std::max(frame_data.camera.get_z_near(), 0.05F);
        const float far_plane = std::max(
            near_plane + 1.0F,
            std::min(frame_data.camera.get_z_far(), frame_data.shadow_render_distance));
        constexpr float split_lambda = 0.65F;

        auto splits = std::vector<float> {};
        splits.reserve(TBX_FORWARD_SHADOW_CASCADE_COUNT);
        for (uint32 cascade = 1U; cascade <= TBX_FORWARD_SHADOW_CASCADE_COUNT; ++cascade)
        {
            const float ratio =
                static_cast<float>(cascade) / static_cast<float>(TBX_FORWARD_SHADOW_CASCADE_COUNT);
            const float logarithmic = near_plane * std::pow(far_plane / near_plane, ratio);
            const float uniform = near_plane + ((far_plane - near_plane) * ratio);
            splits.push_back((logarithmic * split_lambda) + (uniform * (1.0F - split_lambda)));
        }
        return splits;
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

    Result BuildDirectionalShadowCommandsOperation::ensure_shadow_resources(
        IGraphicsBackend& backend,
        const FrameData& frame_data)
    {
        const uint32 resolution = clamp_shadow_resolution(frame_data.shadow_map_resolution);
        if (_shadow_resolution == resolution
            && _shadow_textures.size() == TBX_FORWARD_SHADOW_CASCADE_COUNT)
            return {};

        for (const Uuid& texture : _shadow_textures)
            if (texture.is_valid())
                backend.unload(texture);
        _shadow_textures.clear();
        _shadow_resolution = resolution;

        for (uint32 index = 0U; index < TBX_FORWARD_SHADOW_CASCADE_COUNT; ++index)
        {
            auto texture = Uuid {};
            if (const auto result = backend.upload_texture(
                    GraphicsTextureDesc {
                        .usage = GraphicsTextureUsage::SAMPLED_DEPTH_STENCIL,
                        .format = GraphicsTextureFormat::DEPTH32_FLOAT,
                        .size = Size {resolution, resolution},
                        .mip_count = 1U,
                        .array_layer_count = 1U,
                        .debug_name = "Toybox Directional Shadow Cascade "
                                      + std::to_string(index),
                    },
                    nullptr,
                    0U,
                    texture);
                !result)
                return result;

            _shadow_textures.push_back(texture);
        }

        return {};
    }

    Result BuildDirectionalShadowCommandsOperation::ensure_shadow_uniform_buffer(
        IGraphicsBackend& backend,
        RenderData& render_data)
    {
        auto uniforms = ForwardShadowUniformBlock {
            .shadow_counts =
                IVec4(static_cast<int>(render_data.directional_shadow_cascades.size()), 0, 0, 0),
            .shadow_settings =
                Vec4(std::clamp(render_data.frame.shadow_softness, 0.0F, 3.0F), 0.0F, 0.0F, 0.0F),
            .camera_forward = Vec4(
                normalize(render_data.frame.camera_transform.rotation * Vec3(0.0F, 0.0F, -1.0F)),
                0.0F),
        };

        for (uint32 index = 0U;
             index < render_data.directional_shadow_cascades.size()
             && index < TBX_FORWARD_SHADOW_CASCADE_COUNT;
             ++index)
        {
            const auto& cascade = render_data.directional_shadow_cascades[index];
            uniforms.directional_cascades[index] = ForwardShadowCascade {
                .light_view_projection = cascade.light_view_projection,
                .split_bias_blend = Vec4(
                    cascade.split_depth,
                    cascade.normal_bias,
                    cascade.depth_bias,
                    cascade.blend_distance),
            };
        }

        const auto data_size = static_cast<uint64>(sizeof(ForwardShadowUniformBlock));
        if (!_shadow_uniform_buffer.is_valid())
        {
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Forward Shadow Uniforms",
                    },
                    &uniforms,
                    data_size,
                    _shadow_uniform_buffer);
                !result)
                return result;
        }
        else if (const auto result =
                     backend.update_buffer(_shadow_uniform_buffer, &uniforms, data_size, 0U);
                 !result)
        {
            return result;
        }

        render_data.forward_shadow_uniform_buffer = _shadow_uniform_buffer;
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
        render_data.directional_shadow_passes.clear();
        auto& backend = render_data.frame.backend.get();

        if (render_data.directional_lights.empty())
            return {};

        if (const auto result = ensure_shadow_pipeline(backend); !result)
            return result;
        if (const auto result = ensure_shadow_resources(backend, render_data.frame); !result)
            return result;

        const auto splits = make_cascade_splits(render_data.frame);
        float split_near = std::max(render_data.frame.camera.get_z_near(), 0.05F);
        float previous_split_far = split_near;
        const auto& shadow_light = render_data.directional_lights.front();
        for (uint32 cascade_index = 0U; cascade_index < TBX_FORWARD_SHADOW_CASCADE_COUNT;
             ++cascade_index)
        {
            const float split_far = splits[cascade_index];
            render_data.directional_shadow_cascades.push_back(make_shadow_cascade(
                render_data.frame,
                shadow_light,
                split_near,
                split_far,
                previous_split_far,
                _shadow_resolution,
                _shadow_textures[cascade_index]));
            previous_split_far = split_far;
            split_near = split_far;
        }

        auto& resource_manager = render_data.frame.resource_manager.get();
        auto batches = std::unordered_map<uint64, ShadowBatch> {};
        auto prepare_result = Result {};

        for (const auto& renderable : render_data.renderables)
        {
            if (!prepare_result)
                break;
            if (!renderable.is_visible)
                continue;

            auto material_resource = GraphicsMaterialInstanceResource {};
            prepare_result =
                resource_manager.load_material_instance(renderable.material, material_resource);
            if (!prepare_result)
                break;
            if (!material_casts_standard_shadow(material_resource.config))
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

                const uint64 batch_key =
                    make_dynamic_batch_key(make_mesh_cache_key(renderable.dynamic_mesh), 0U);
                auto& batch = batches[batch_key];
                batch.vertex_buffer = vertex_buffer;
                batch.index_buffer = index_buffer;
                batch.index_count = index_count;
                batch.transforms.push_back(model_to_world);
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
                const uint64 batch_key = make_static_batch_key(
                    mesh.vertex_buffer,
                    mesh.index_buffer,
                    mesh.index_count,
                    0U);
                auto& batch = batches[batch_key];
                batch.vertex_buffer = mesh.vertex_buffer;
                batch.index_buffer = mesh.index_buffer;
                batch.index_count = mesh.index_count;
                batch.transforms.push_back(model_to_world);
            }
        }

        if (!prepare_result)
            return prepare_result;

        for (auto& [batch_key, batch] : batches)
        {
            if (batch.transforms.empty())
                continue;
            if (const auto result = ensure_instance_buffer(
                    backend,
                    batch_key,
                    batch.transforms,
                    batch.instance_buffer);
                !result)
                return result;
        }

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

            auto pass = GraphicsRenderPass {
                .pass =
                    GraphicsPassDesc {
                        .depth_stencil_target = cascade.texture,
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

            for (auto& [batch_key, batch] : batches)
            {
                (void)batch_key;
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

        return ensure_shadow_uniform_buffer(backend, render_data);
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
        for (const Uuid& texture : _shadow_textures)
            if (texture.is_valid())
                backend.unload(texture);
        for (const Uuid& buffer : _shadow_view_uniform_buffers)
            if (buffer.is_valid())
                backend.unload(buffer);
        if (_shadow_pipeline.is_valid())
            backend.unload(_shadow_pipeline);
        if (_shadow_uniform_buffer.is_valid())
            backend.unload(_shadow_uniform_buffer);

        _mesh_vertex_buffers.clear();
        _mesh_index_buffers.clear();
        _mesh_index_counts.clear();
        _instance_buffers.clear();
        _instance_buffer_sizes.clear();
        _shadow_textures.clear();
        _shadow_view_uniform_buffers.clear();
        _shadow_pipeline = {};
        _shadow_uniform_buffer = {};
        _shadow_resolution = 0U;
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

    Result BuildOpaqueCommandsOperation::ensure_lighting_uniform_buffer(
        IGraphicsBackend& backend,
        const RenderData& render_data,
        Uuid& out_buffer)
    {
        out_buffer = {};

        const auto uniforms = make_forward_lighting_uniforms(render_data);
        const auto data_size = static_cast<uint64>(sizeof(ForwardLightingUniformBlock));
        if (!_lighting_uniform_buffer.is_valid())
        {
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Forward Lighting Uniforms",
                    },
                    &uniforms,
                    data_size,
                    _lighting_uniform_buffer);
                !result)
                return result;

            out_buffer = _lighting_uniform_buffer;
            return {};
        }

        if (const auto result =
                backend.update_buffer(_lighting_uniform_buffer, &uniforms, data_size, 0U);
            !result)
            return result;

        out_buffer = _lighting_uniform_buffer;
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

    static void append_directional_shadow_texture_bindings(
        const RenderData& render_data,
        std::vector<GraphicsResourceBinding>& textures)
    {
        for (uint32 index = 0U;
             index < render_data.directional_shadow_cascades.size()
             && index < TBX_FORWARD_SHADOW_CASCADE_COUNT;
             ++index)
        {
            const Uuid texture = render_data.directional_shadow_cascades[index].texture;
            if (!texture.is_valid())
                continue;

            textures.push_back(
                GraphicsResourceBinding {
                    .slot = TBX_FORWARD_SHADOW_TEXTURE_BINDING + index,
                    .resource = texture,
                });
        }
    }

    static std::vector<GraphicsResourceBinding> make_scene_uniform_bindings(
        const Uuid view_uniform_buffer,
        const Uuid material_uniform_buffer,
        const Uuid lighting_uniform_buffer,
        const Uuid shadow_uniform_buffer)
    {
        auto uniforms = std::vector<GraphicsResourceBinding> {
            GraphicsResourceBinding {.slot = 0U, .resource = view_uniform_buffer},
            GraphicsResourceBinding {.slot = 1U, .resource = material_uniform_buffer},
            GraphicsResourceBinding {.slot = 7U, .resource = lighting_uniform_buffer},
        };

        if (shadow_uniform_buffer.is_valid())
        {
            uniforms.push_back(
                GraphicsResourceBinding {.slot = 8U, .resource = shadow_uniform_buffer});
        }

        return uniforms;
    }

    Result BuildOpaqueCommandsOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data.frame;
        render_data.opaque_commands.clear();
        auto& backend = frame_data.backend.get();
        auto& resource_manager = frame_data.resource_manager.get();
        const RenderData& scene_data = render_data;
        const Uuid view_uniform_buffer = frame_data.view_uniform_buffer;
        auto lighting_uniform_buffer = Uuid {};
        if (const auto result =
                ensure_lighting_uniform_buffer(backend, render_data, lighting_uniform_buffer);
            !result)
            return result;

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
            append_directional_shadow_texture_bindings(render_data, textures);

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
                        batch.material_uniform_buffer,
                        lighting_uniform_buffer,
                        render_data.forward_shadow_uniform_buffer),
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
            append_directional_shadow_texture_bindings(render_data, textures);

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
                        batch.material_uniform_buffer,
                        lighting_uniform_buffer,
                        render_data.forward_shadow_uniform_buffer),
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

        if (_lighting_uniform_buffer.is_valid())
            backend.unload(_lighting_uniform_buffer);
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
        _lighting_uniform_buffer = {};
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
}
