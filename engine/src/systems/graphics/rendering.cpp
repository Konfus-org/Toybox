#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/resource_manager.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/math/matrices.h"
#include <cmath>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    struct GeometryBuildResult
    {
        std::vector<float> vertices = {};
        std::vector<uint32> indices = {};
    };

    struct RenderView
    {
        Camera camera = {};
        Transform transform = {};
    };

    struct MeshInstanceDrawBatch
    {
        std::shared_ptr<Mesh> mesh_data = {};
        std::vector<Mat4> world_to_clip_transforms = {};
    };

    struct MaterialUniformBlock
    {
        Color color = Color::WHITE;
        Color emissive = Color::BLACK;
        float specular_strength = 0.5F;
        float shininess_strength = 32.0F;
        float alpha_cutoff = 0.1F;
        float padding0 = 0.0F;
        float transparency_amount = 0.0F;
        float exposure = 1.0F;
        float diffuse_strength = 1.0F;
        float normal_strength = 1.0F;
        float emissive_strength = 1.0F;
        float color_texture_blend = 1.0F;
        float wireframe_width = 1.0F;
        float padding1 = 0.0F;
        float padding2 = 0.0F;
        float padding3 = 0.0F;
    };

    struct ViewUniformBlock
    {
        Mat4 view_projection = Mat4(1.0F);
    };

    struct MaterialDrawState
    {
        uint64 key = 0U;
        Uuid pipeline = {};
        Uuid uniform_buffer = {};
        std::vector<GraphicsResourceBinding> textures = {};
    };

    struct MaterialMeshInstanceDrawBatch
    {
        std::shared_ptr<Mesh> mesh_data = {};
        MaterialDrawState material = {};
        std::vector<Mat4> model_to_world_transforms = {};
    };

    struct StaticMaterialDrawBatch
    {
        Uuid vertex_buffer = {};
        Uuid index_buffer = {};
        uint32 index_count = 0U;
        MaterialDrawState material = {};
        std::vector<Mat4> model_to_world_transforms = {};
    };

    static uint64 hash_bytes(const void* data, const uint64 size, uint64 hash = 14695981039346656037ULL)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (uint64 index = 0U; index < size; ++index)
        {
            hash ^= bytes[index];
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static uint64 hash_value(const uint64 value, const uint64 hash)
    {
        return hash_bytes(&value, static_cast<uint64>(sizeof(value)), hash);
    }

    static uint64 hash_uuid(const Uuid value, const uint64 hash)
    {
        return hash_value(static_cast<uint32>(value), hash);
    }

    template <typename TValue>
    static TValue get_parameter_or(
        const MaterialParameterBindings& parameters,
        const std::string_view name,
        const TValue& fallback)
    {
        const auto parameter = parameters.get(name);
        if (!parameter.has_value())
            return fallback;

        if (const auto* value = std::get_if<TValue>(&parameter->get().data))
            return *value;

        return fallback;
    }

    static MaterialUniformBlock make_material_uniform_block(
        const MaterialParameterBindings& parameters)
    {
        auto block = MaterialUniformBlock {};
        block.color = get_parameter_or(parameters, "color", block.color);
        block.emissive = get_parameter_or(parameters, "emissive", block.emissive);
        block.specular_strength =
            get_parameter_or(parameters, "specular_strength", block.specular_strength);
        block.shininess_strength =
            get_parameter_or(parameters, "shininess_strength", block.shininess_strength);
        block.alpha_cutoff = get_parameter_or(parameters, "alpha_cutoff", block.alpha_cutoff);
        block.transparency_amount =
            get_parameter_or(parameters, "transparency_amount", block.transparency_amount);
        block.exposure = get_parameter_or(parameters, "exposure", block.exposure);
        block.diffuse_strength =
            get_parameter_or(parameters, "diffuse_strength", block.diffuse_strength);
        block.normal_strength =
            get_parameter_or(parameters, "normal_strength", block.normal_strength);
        block.emissive_strength =
            get_parameter_or(parameters, "emissive_strength", block.emissive_strength);
        block.color_texture_blend =
            get_parameter_or(parameters, "color_texture_blend", block.color_texture_blend);
        block.wireframe_width =
            get_parameter_or(parameters, "wireframe_width", block.wireframe_width);
        return block;
    }

    static uint64 make_material_key(
        const Uuid pipeline,
        const MaterialUniformBlock& uniforms,
        const std::vector<GraphicsResourceBinding>& textures)
    {
        uint64 hash = hash_uuid(pipeline, 14695981039346656037ULL);
        hash = hash_bytes(&uniforms, static_cast<uint64>(sizeof(uniforms)), hash);
        for (const auto& texture : textures)
        {
            hash = hash_value(texture.slot, hash);
            hash = hash_uuid(texture.resource, hash);
        }
        return hash == 0U ? 1U : hash;
    }

    static uint64 make_dynamic_material_batch_key(
        const uint64 mesh_key,
        const uint64 material_key)
    {
        return hash_value(material_key, hash_value(mesh_key, 14695981039346656037ULL));
    }

    static uint64 make_static_material_batch_key(
        const Uuid vertex_buffer,
        const Uuid index_buffer,
        const uint32 index_count,
        const uint64 material_key)
    {
        uint64 hash = hash_uuid(vertex_buffer, 14695981039346656037ULL);
        hash = hash_uuid(index_buffer, hash);
        hash = hash_value(index_count, hash);
        hash = hash_value(material_key, hash);
        return hash == 0U ? 1U : hash;
    }

    static Result resolve_material_textures(
        const MaterialTextureBindings& textures,
        GraphicsResourceManager& resource_manager,
        std::vector<GraphicsResourceBinding>& out_textures)
    {
        auto texture_slot = uint32 {0U};
        out_textures.reserve(textures.values.size());
        for (const auto& texture : textures)
        {
            auto texture_resource = Uuid {};
            if (texture.texture.is_valid())
            {
                if (const auto result =
                        resource_manager.load_texture(texture.texture, texture_resource);
                    !result)
                    return result;
            }
            else if (const auto result = resource_manager.load_default_texture(texture_resource);
                     !result)
            {
                return result;
            }

            out_textures.push_back(
                GraphicsResourceBinding {
                    .slot = texture_slot,
                    .resource = texture_resource,
                });
            texture_slot += 1U;
        }

        return {};
    }

    static Result resolve_material_draw_state(
        const MaterialInstance& material,
        GraphicsResourceManager& resource_manager,
        const std::function<Result(uint64, const void*, uint64, Uuid&)>& ensure_uniform_buffer,
        MaterialDrawState& out_material)
    {
        out_material = {};
        auto material_resource = GraphicsMaterialInstanceResource {};
        if (const auto result = resource_manager.load_material_instance(material, material_resource);
            !result)
            return result;

        auto material_uniforms = make_material_uniform_block(material_resource.parameters);
        auto texture_bindings = std::vector<GraphicsResourceBinding> {};
        if (const auto result = resolve_material_textures(
                material_resource.textures,
                resource_manager,
                texture_bindings);
            !result)
            return result;

        const uint64 material_key =
            make_material_key(material_resource.pipeline, material_uniforms, texture_bindings);
        auto material_uniform_buffer = Uuid {};
        if (const auto result = ensure_uniform_buffer(
                material_key,
                &material_uniforms,
                static_cast<uint64>(sizeof(material_uniforms)),
                material_uniform_buffer);
            !result)
            return result;

        out_material = MaterialDrawState {
            .key = material_key,
            .pipeline = material_resource.pipeline,
            .uniform_buffer = material_uniform_buffer,
            .textures = std::move(texture_bindings),
        };
        return {};
    }

    static Shader make_geometry_shader()
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

    static GraphicsPipelineDesc make_geometry_pipeline_desc()
    {
        return GraphicsPipelineDesc {
            .shader = make_geometry_shader(),
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
            .debug_name = "Toybox Geometry Pass Pipeline",
        };
    }

    static Shader make_dynamic_mesh_shader()
    {
        return Shader(
            std::vector<ShaderSource> {
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) in vec3 a_position;\n"
                    "layout(location = 5) in vec4 a_world_to_clip0;\n"
                    "layout(location = 6) in vec4 a_world_to_clip1;\n"
                    "layout(location = 7) in vec4 a_world_to_clip2;\n"
                    "layout(location = 8) in vec4 a_world_to_clip3;\n"
                    "out vec3 v_color;\n"
                    "void main()\n"
                    "{\n"
                    "    mat4 world_to_clip = mat4(\n"
                    "        a_world_to_clip0,\n"
                    "        a_world_to_clip1,\n"
                    "        a_world_to_clip2,\n"
                    "        a_world_to_clip3);\n"
                    "    v_color = (a_position * 0.5) + vec3(0.5, 0.5, 0.75);\n"
                    "    gl_Position = world_to_clip * vec4(a_position, 1.0);\n"
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

    static GraphicsPipelineDesc make_dynamic_mesh_pipeline_desc()
    {
        return GraphicsPipelineDesc {
            .shader = make_dynamic_mesh_shader(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = static_cast<uint32>(sizeof(float) * 16U),
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
            .is_culling_enabled = false,
            .debug_name = "Toybox Dynamic Mesh Instance Pipeline",
        };
    }

    static Shader make_model_shader()
    {
        return Shader(
            std::vector<ShaderSource> {
                ShaderSource(
                    "#version 450 core\n"
                    "layout(location = 0) in vec3 a_position;\n"
                    "layout(std140, binding = 0) uniform ToyboxObjectBlock\n"
                    "{\n"
                    "    mat4 u_world_to_clip;\n"
                    "};\n"
                    "out vec3 v_color;\n"
                    "void main()\n"
                    "{\n"
                    "    v_color = (a_position * 0.5) + vec3(0.5, 0.5, 0.75);\n"
                    "    gl_Position = u_world_to_clip * vec4(a_position, 1.0);\n"
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

    static GraphicsPipelineDesc make_model_pipeline_desc()
    {
        return GraphicsPipelineDesc {
            .shader = make_model_shader(),
            .vertex_buffers =
                {
                    GraphicsVertexBufferLayoutDesc {
                        .slot = 0U,
                        .stride = static_cast<uint32>(sizeof(float) * 16U),
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
            .debug_name = "Toybox Model Pipeline",
        };
    }

    static uint32 get_vertex_stride_float_count(const Mesh& mesh)
    {
        const uint32 stride_bytes = mesh.vertices.layout.stride;
        if (stride_bytes == 0U)
            return 16U;

        return stride_bytes / static_cast<uint32>(sizeof(float));
    }

    static bool can_render_mesh_directly(const Mesh& mesh)
    {
        constexpr uint32 model_pipeline_stride = 16U;
        return !mesh.vertices.empty() && !mesh.indices.empty()
               && get_vertex_stride_float_count(mesh) == model_pipeline_stride;
    }

    static uint64 make_mesh_cache_key(const std::shared_ptr<Mesh>& mesh_data)
    {
        return reinterpret_cast<uint64>(mesh_data.get());
    }

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
        const Vec3 projected_position = Vec3(
            clip_position.x * inverse_w,
            clip_position.y * inverse_w,
            clip_position.z * inverse_w);

        geometry.vertices.push_back(projected_position.x);
        geometry.vertices.push_back(projected_position.y);
        geometry.vertices.push_back(projected_position.z);
    }

    static void append_clipped_triangle(
        const Vec4& clip_position0,
        const Vec4& clip_position1,
        const Vec4& clip_position2,
        GeometryBuildResult& geometry)
    {
        auto polygon = std::vector<Vec4> {clip_position0, clip_position1, clip_position2};
        for (uint32 plane_index = 0U; plane_index < 6U; ++plane_index)
        {
            polygon = clip_polygon_against_plane(polygon, plane_index);
            if (polygon.size() < 3U)
                return;
        }

        for (uint32 polygon_index = 1U; polygon_index + 1U < polygon.size(); ++polygon_index)
        {
            const uint32 base_vertex = static_cast<uint32>(geometry.vertices.size() / 3U);
            append_projected_vertex(polygon[0U], geometry);
            append_projected_vertex(polygon[polygon_index], geometry);
            append_projected_vertex(polygon[polygon_index + 1U], geometry);
            geometry.indices.push_back(base_vertex);
            geometry.indices.push_back(base_vertex + 1U);
            geometry.indices.push_back(base_vertex + 2U);
        }
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
            const uint32 source_offset = vertex_index * stride;
            const Vec4 local_position = Vec4(
                mesh.vertices.vertices[source_offset],
                mesh.vertices.vertices[source_offset + 1U],
                mesh.vertices.vertices[source_offset + 2U],
                1.0F);
            clip_positions.push_back(world_to_clip * local_position);
        }

        for (uint32 index_offset = 0U; index_offset + 2U < mesh.indices.size(); index_offset += 3U)
        {
            const uint32 index0 = mesh.indices[index_offset];
            const uint32 index1 = mesh.indices[index_offset + 1U];
            const uint32 index2 = mesh.indices[index_offset + 2U];
            if (index0 >= source_vertex_count || index1 >= source_vertex_count
                || index2 >= source_vertex_count)
                continue;

            append_clipped_triangle(
                clip_positions[index0],
                clip_positions[index1],
                clip_positions[index2],
                geometry);
        }
    }

    static RenderView find_render_camera(EntityRegistry& entity_registry, Size resolution)
    {
        auto render_camera = RenderView {
            .transform = Transform(Vec3(0.0F, 2.0F, 8.0F)),
        };
        auto found_camera = false;
        entity_registry.for_each_with<Camera, Transform>(
            [&render_camera, &found_camera](Entity& entity)
            {
                if (found_camera)
                    return;

                render_camera.camera = entity.get_component<Camera>();
                render_camera.transform = get_world_space_transform(entity);
                found_camera = true;
            });

        const float aspect = resolution.height == 0U ? 1.0F
                                                     : static_cast<float>(resolution.width)
                                                           / static_cast<float>(resolution.height);
        render_camera.camera.set_aspect(aspect);
        return render_camera;
    }

    static void touch_material_resources(
        const MaterialInstance& material,
        GraphicsResourceManager& resource_manager)
    {
        const Handle& material_handle = material.get_handle();
        if (material_handle.is_valid())
        {
            auto material_resource = Uuid {};
            resource_manager.load_material(material_handle, material_resource);
        }

        for (const auto& texture_override : material.texture_overrides)
        {
            if (!texture_override.texture.is_valid())
                continue;

            auto texture_resource = Uuid {};
            resource_manager.load_texture(texture_override.texture, texture_resource);
        }
    }

    static void touch_material_resources_once(
        const MaterialInstance& material,
        GraphicsResourceManager& resource_manager,
        std::unordered_set<Handle>& touched_materials,
        std::unordered_set<Handle>& touched_textures)
    {
        const Handle& material_handle = material.get_handle();
        if (material_handle.is_valid() && touched_materials.insert(material_handle).second)
        {
            auto material_resource = Uuid {};
            resource_manager.load_material(material_handle, material_resource);
        }

        for (const auto& texture_override : material.texture_overrides)
        {
            if (!texture_override.texture.is_valid())
                continue;
            if (!touched_textures.insert(texture_override.texture).second)
                continue;

            auto texture_resource = Uuid {};
            resource_manager.load_texture(texture_override.texture, texture_resource);
        }
    }

    Rendering::Rendering(
        IGraphicsBackend& backend,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        IWindowManager& window_manager,
        Window output_window,
        const GraphicsSettings& settings)
        : _backend(backend)
        , _entity_registry(entity_registry)
        , _window_manager(window_manager)
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _resource_manager(std::make_unique<GraphicsResourceManager>(backend, asset_manager))
        , _pipeline(std::make_unique<GraphicsRenderPipeline>(backend))
    {
        _initialization_result = backend.initialize(settings);
        setup_geometry_pass(0U, {});
    }

    Rendering::~Rendering() noexcept
    {
        release_resources();
    }

    void Rendering::render()
    {
        if (!_initialization_result)
        {
            TBX_TRACE_ERROR(
                "Toybox renderer initialization failed: {}",
                _initialization_result.get_report());
            return;
        }
        if (!_output_window.is_valid() || !_window_manager.get().is_open(_output_window))
            return;

        _render_frame += 1U;
        if (_resource_manager)
            _resource_manager->update();

        if (const auto result = begin_frame_and_view(); !result)
        {
            TBX_TRACE_WARNING("Toybox renderer frame submission failed: {}", result.get_report());
            return;
        }

        const auto finish_after_failure = [this](const Result& failure)
        {
            auto& backend = _backend.get();
            backend.end_view();
            backend.end_frame();
            TBX_TRACE_WARNING("Toybox renderer frame submission failed: {}", failure.get_report());
        };

        if (const auto result = ensure_model_pipeline(); !result)
        {
            finish_after_failure(result);
            return;
        }

        const Size resolution = get_render_resolution();
        const RenderView render_view = find_render_camera(_entity_registry.get(), resolution);
        const Mat4 view_projection = render_view.camera.get_view_projection_matrix(
            render_view.transform.position,
            render_view.transform.rotation);
        auto view_uniform_buffer = Uuid {};
        if (const auto result = ensure_view_uniform_buffer(view_projection, view_uniform_buffer);
            !result)
        {
            finish_after_failure(result);
            return;
        }

        auto fallback_vertices = std::vector<float> {};
        auto fallback_indices = std::vector<uint32> {};
        auto indexed_draws = std::vector<GraphicsIndexedDrawCommand> {};
        if (const auto result = append_dynamic_mesh_draws(
                view_projection,
                view_uniform_buffer,
                indexed_draws,
                fallback_vertices,
                fallback_indices);
            !result)
        {
            finish_after_failure(result);
            return;
        }
        if (const auto result =
                append_static_model_draws(view_projection, view_uniform_buffer, indexed_draws);
            !result)
        {
            finish_after_failure(result);
            return;
        }

        if (!fallback_indices.empty())
        {
            if (const auto result = ensure_geometry_pipeline(); !result)
            {
                finish_after_failure(result);
                return;
            }
        }

        if (const auto result = ensure_geometry_buffers(fallback_vertices, fallback_indices);
            !result)
        {
            finish_after_failure(result);
            return;
        }

        unload_stale_dynamic_mesh_buffers();
        unload_stale_material_uniform_buffers();
        unload_stale_model_transform_buffers();
        setup_geometry_pass(static_cast<uint32>(fallback_indices.size()), std::move(indexed_draws));
        if (const auto result = _pipeline->execute(); !result)
        {
            finish_after_failure(result);
            return;
        }

        if (const auto result = end_view_and_frame(); !result)
        {
            TBX_TRACE_WARNING("Toybox renderer frame submission failed: {}", result.get_report());
        }
    }

    Result Rendering::begin_frame_and_view()
    {
        const Size resolution = get_render_resolution();
        const auto frame = GraphicsFrameInfo {
            .output_window = _output_window,
            .render_resolution = resolution,
            .output_resolution = resolution,
        };
        auto& backend = _backend.get();
        if (const auto result = backend.begin_frame(frame); !result)
            return result;

        const RenderView render_camera = find_render_camera(_entity_registry.get(), resolution);
        const auto graphics_view = GraphicsView {
            .camera = render_camera.camera,
            .viewport =
                Viewport {
                    .position = Vec2(0.0F),
                    .dimensions = resolution,
                },
        };
        if (const auto result = backend.begin_view(graphics_view); !result)
        {
            backend.end_frame();
            return result;
        }

        if (const auto result = backend.set_viewport(graphics_view.viewport); !result)
        {
            backend.end_view();
            backend.end_frame();
            return result;
        }

        return {};
    }

    Result Rendering::end_view_and_frame()
    {
        auto& backend = _backend.get();
        if (const auto result = backend.end_view(); !result)
            return result;
        if (const auto result = backend.present(); !result)
            return result;

        return backend.end_frame();
    }

    Result Rendering::ensure_geometry_buffers(
        const std::vector<float>& vertices,
        const std::vector<uint32>& indices)
    {
        if (vertices.empty() || indices.empty())
            return {};

        auto& backend = _backend.get();
        const uint64 vertex_data_size =
            static_cast<uint64>(vertices.size()) * static_cast<uint64>(sizeof(float));
        if (!_geometry_vertex_buffer.is_valid() || vertex_data_size > _geometry_vertex_buffer_size)
        {
            if (_geometry_vertex_buffer.is_valid())
                backend.unload(_geometry_vertex_buffer);

            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::VERTEX,
                        .size = vertex_data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Geometry Vertices",
                    },
                    vertices.data(),
                    vertex_data_size,
                    _geometry_vertex_buffer);
                !result)
                return result;

            _geometry_vertex_buffer_size = vertex_data_size;
        }
        else if (
            const auto result =
                backend
                    .update_buffer(_geometry_vertex_buffer, vertices.data(), vertex_data_size, 0U);
            !result)
        {
            return result;
        }

        const uint64 index_data_size =
            static_cast<uint64>(indices.size()) * static_cast<uint64>(sizeof(uint32));
        if (!_geometry_index_buffer.is_valid() || index_data_size > _geometry_index_buffer_size)
        {
            if (_geometry_index_buffer.is_valid())
                backend.unload(_geometry_index_buffer);

            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::INDEX,
                        .size = index_data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Geometry Indices",
                    },
                    indices.data(),
                    index_data_size,
                    _geometry_index_buffer);
                !result)
                return result;

            _geometry_index_buffer_size = index_data_size;
        }
        else if (
            const auto result =
                backend.update_buffer(_geometry_index_buffer, indices.data(), index_data_size, 0U);
            !result)
        {
            return result;
        }

        return {};
    }

    Result Rendering::ensure_geometry_pipeline()
    {
        if (_geometry_pipeline.is_valid())
            return {};

        return _backend.get().upload_pipeline(make_geometry_pipeline_desc(), _geometry_pipeline);
    }

    Result Rendering::ensure_dynamic_mesh_instance_buffer(
        const uint64 mesh_key,
        const std::vector<Mat4>& world_to_clip_transforms,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (world_to_clip_transforms.empty())
            return {};

        const uint64 data_size = static_cast<uint64>(world_to_clip_transforms.size())
                                 * static_cast<uint64>(sizeof(Mat4));
        auto& backend = _backend.get();
        const auto buffer_iterator = _dynamic_mesh_instance_buffers.find(mesh_key);
        const auto size_iterator = _dynamic_mesh_instance_buffer_sizes.find(mesh_key);
        if (buffer_iterator == _dynamic_mesh_instance_buffers.end()
            || size_iterator == _dynamic_mesh_instance_buffer_sizes.end()
            || data_size > size_iterator->second)
        {
            if (buffer_iterator != _dynamic_mesh_instance_buffers.end())
                backend.unload(buffer_iterator->second);

            auto buffer = Uuid {};
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::VERTEX,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Dynamic Mesh Instance Transforms",
                    },
                    world_to_clip_transforms.data(),
                    data_size,
                    buffer);
                !result)
            {
                return result;
            }

            _dynamic_mesh_instance_buffers[mesh_key] = buffer;
            _dynamic_mesh_instance_buffer_sizes[mesh_key] = data_size;
            _dynamic_mesh_last_access_frames[mesh_key] = _render_frame;
            out_buffer = buffer;
            return {};
        }

        if (const auto result = backend.update_buffer(
                buffer_iterator->second,
                world_to_clip_transforms.data(),
                data_size,
                0U);
            !result)
        {
            return result;
        }

        out_buffer = buffer_iterator->second;
        _dynamic_mesh_last_access_frames[mesh_key] = _render_frame;
        return {};
    }

    Result Rendering::ensure_material_uniform_buffer(
        const uint64 material_key,
        const void* material_data,
        const uint64 material_data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (material_key == 0U || material_data == nullptr || material_data_size == 0U)
            return Result(false, "Rendering: material uniform data is invalid.");

        auto iterator = _material_uniform_buffers.find(material_key);
        if (iterator != _material_uniform_buffers.end())
        {
            out_buffer = iterator->second;
            _material_uniform_last_access_frames[material_key] = _render_frame;
            return {};
        }

        auto buffer = Uuid {};
        if (const auto result = _backend.get().upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::UNIFORM,
                    .size = material_data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Material Uniforms",
                },
                material_data,
                material_data_size,
                buffer);
            !result)
        {
            return result;
        }

        _material_uniform_buffers[material_key] = buffer;
        _material_uniform_last_access_frames[material_key] = _render_frame;
        out_buffer = buffer;
        return {};
    }

    Result Rendering::ensure_view_uniform_buffer(
        const Mat4& view_projection,
        Uuid& out_buffer)
    {
        out_buffer = {};
        const auto view_block = ViewUniformBlock {
            .view_projection = view_projection,
        };
        const uint64 data_size = static_cast<uint64>(sizeof(view_block));
        if (!_view_uniform_buffer.is_valid())
        {
            if (const auto result = _backend.get().upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox View Uniforms",
                    },
                    &view_block,
                    data_size,
                    _view_uniform_buffer);
                !result)
            {
                return result;
            }

            out_buffer = _view_uniform_buffer;
            return {};
        }

        if (const auto result =
                _backend.get().update_buffer(_view_uniform_buffer, &view_block, data_size, 0U);
            !result)
        {
            return result;
        }

        out_buffer = _view_uniform_buffer;
        return {};
    }

    Result Rendering::ensure_dynamic_mesh_pipeline()
    {
        if (_dynamic_mesh_pipeline.is_valid())
            return {};

        return _backend.get().upload_pipeline(
            make_dynamic_mesh_pipeline_desc(),
            _dynamic_mesh_pipeline);
    }

    Result Rendering::ensure_model_pipeline()
    {
        if (_model_pipeline.is_valid())
            return {};

        return _backend.get().upload_pipeline(make_model_pipeline_desc(), _model_pipeline);
    }

    Result Rendering::ensure_dynamic_mesh_buffers(
        const std::shared_ptr<Mesh>& mesh_data,
        Uuid& out_vertex_buffer,
        Uuid& out_index_buffer,
        uint32& out_index_count)
    {
        out_vertex_buffer = {};
        out_index_buffer = {};
        out_index_count = 0U;
        if (!mesh_data || !can_render_mesh_directly(*mesh_data))
            return Result(false, "Rendering: dynamic mesh is not uploadable.");

        const uint64 mesh_key = make_mesh_cache_key(mesh_data);
        const auto vertex_iterator = _dynamic_mesh_vertex_buffers.find(mesh_key);
        const auto index_iterator = _dynamic_mesh_index_buffers.find(mesh_key);
        const auto count_iterator = _dynamic_mesh_index_counts.find(mesh_key);
        if (vertex_iterator != _dynamic_mesh_vertex_buffers.end()
            && index_iterator != _dynamic_mesh_index_buffers.end()
            && count_iterator != _dynamic_mesh_index_counts.end())
        {
            _dynamic_mesh_last_access_frames[mesh_key] = _render_frame;
            out_vertex_buffer = vertex_iterator->second;
            out_index_buffer = index_iterator->second;
            out_index_count = count_iterator->second;
            return {};
        }

        const uint64 vertex_data_size =
            static_cast<uint64>(mesh_data->vertices.size()) * static_cast<uint64>(sizeof(float));
        const uint64 index_data_size =
            static_cast<uint64>(mesh_data->indices.size()) * static_cast<uint64>(sizeof(uint32));

        auto vertex_buffer = Uuid {};
        auto& backend = _backend.get();
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::VERTEX,
                    .size = vertex_data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Dynamic Mesh Vertices",
                },
                mesh_data->vertices.data(),
                vertex_data_size,
                vertex_buffer);
            !result)
        {
            return result;
        }

        auto index_buffer = Uuid {};
        if (const auto result = backend.upload_buffer(
                GraphicsBufferDesc {
                    .usage = GraphicsBufferUsage::INDEX,
                    .size = index_data_size,
                    .is_dynamic = false,
                    .debug_name = "Toybox Dynamic Mesh Indices",
                },
                mesh_data->indices.data(),
                index_data_size,
                index_buffer);
            !result)
        {
            backend.unload(vertex_buffer);
            return result;
        }

        _dynamic_mesh_sources[mesh_key] = mesh_data;
        _dynamic_mesh_vertex_buffers[mesh_key] = vertex_buffer;
        _dynamic_mesh_index_buffers[mesh_key] = index_buffer;
        _dynamic_mesh_index_counts[mesh_key] = static_cast<uint32>(mesh_data->indices.size());
        _dynamic_mesh_last_access_frames[mesh_key] = _render_frame;

        out_vertex_buffer = vertex_buffer;
        out_index_buffer = index_buffer;
        out_index_count = static_cast<uint32>(mesh_data->indices.size());
        return {};
    }

    Result Rendering::ensure_model_transform_buffer(
        const Uuid entity_id,
        const Mat4& world_to_clip,
        Uuid& out_buffer)
    {
        out_buffer = {};
        const uint64 data_size = static_cast<uint64>(sizeof(Mat4));
        auto& backend = _backend.get();
        auto iterator = _model_transform_buffers.find(entity_id);
        if (iterator == _model_transform_buffers.end())
        {
            auto buffer = Uuid {};
            if (const auto result = backend.upload_buffer(
                    GraphicsBufferDesc {
                        .usage = GraphicsBufferUsage::UNIFORM,
                        .size = data_size,
                        .is_dynamic = true,
                        .debug_name = "Toybox Model Transform",
                    },
                    &world_to_clip,
                    data_size,
                    buffer);
                !result)
            {
                return result;
            }

            iterator = _model_transform_buffers.emplace(entity_id, buffer).first;
        }
        else if (
            const auto result =
                backend.update_buffer(iterator->second, &world_to_clip, data_size, 0U);
            !result)
        {
            return result;
        }

        out_buffer = iterator->second;
        _model_transform_last_access_frames[entity_id] = _render_frame;
        return {};
    }

    Size Rendering::get_render_resolution() const
    {
        if (_requested_resolution.width > 0U && _requested_resolution.height > 0U)
            return _requested_resolution;

        return _window_manager.get().get_size(_output_window);
    }

    Result Rendering::append_dynamic_mesh_draws(
        const Mat4& view_projection,
        const Uuid& view_uniform_buffer,
        std::vector<GraphicsIndexedDrawCommand>& out_draws,
        std::vector<float>& out_fallback_vertices,
        std::vector<uint32>& out_fallback_indices)
    {
        auto result = Result {};
        auto fallback_geometry = GeometryBuildResult {
            .vertices = std::move(out_fallback_vertices),
            .indices = std::move(out_fallback_indices),
        };
        auto draw_batches = std::unordered_map<uint64, MeshInstanceDrawBatch> {};
        auto material_draw_batches = std::unordered_map<uint64, MaterialMeshInstanceDrawBatch> {};
        auto touched_materials = std::unordered_set<Handle> {};
        auto touched_textures = std::unordered_set<Handle> {};
        const auto ensure_material_uniform =
            [this](const uint64 material_key, const void* data, const uint64 data_size, Uuid& out)
        {
            return ensure_material_uniform_buffer(material_key, data, data_size, out);
        };

        const auto append_mesh_draw =
            [this,
             &fallback_geometry,
             &draw_batches,
             &material_draw_batches,
             &ensure_material_uniform,
             &result,
             &touched_materials,
             &touched_textures,
             &view_projection](Entity& entity, const std::shared_ptr<Mesh>& mesh_data)
        {
            if (!result || !mesh_data)
                return;

            const Mat4 model_to_world = build_transform_matrix(get_world_space_transform(entity));
            const Mat4 world_to_clip = view_projection * model_to_world;
            if (!can_render_mesh_directly(*mesh_data))
            {
                if (entity.has_component<MaterialInstance>())
                    touch_material_resources_once(
                        entity.get_component<MaterialInstance>(),
                        *_resource_manager,
                        touched_materials,
                        touched_textures);

                append_mesh_geometry(*mesh_data, world_to_clip, fallback_geometry);
                return;
            }

            if (entity.has_component<MaterialInstance>())
            {
                auto material = MaterialDrawState {};
                result = resolve_material_draw_state(
                    entity.get_component<MaterialInstance>(),
                    *_resource_manager,
                    ensure_material_uniform,
                    material);
                if (!result)
                    return;

                const uint64 mesh_key = make_mesh_cache_key(mesh_data);
                const uint64 batch_key =
                    make_dynamic_material_batch_key(mesh_key, material.key);
                auto& batch = material_draw_batches[batch_key];
                if (!batch.mesh_data)
                {
                    batch.mesh_data = mesh_data;
                    batch.material = std::move(material);
                }
                batch.model_to_world_transforms.push_back(model_to_world);
                return;
            }

            const uint64 mesh_key = make_mesh_cache_key(mesh_data);
            auto& batch = draw_batches[mesh_key];
            if (!batch.mesh_data)
                batch.mesh_data = mesh_data;
            batch.world_to_clip_transforms.push_back(world_to_clip);
        };

        _entity_registry.get().for_each_with<DynamicMesh, Transform>(
            [&append_mesh_draw](Entity& entity)
            {
                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                append_mesh_draw(entity, dynamic_mesh.data);
            });

        for (const auto& entry : draw_batches)
        {
            const uint64 mesh_key = entry.first;
            const auto& batch = entry.second;
            if (!batch.mesh_data || batch.world_to_clip_transforms.empty())
                continue;

            if (const auto pipeline_result = ensure_dynamic_mesh_pipeline(); !pipeline_result)
                return pipeline_result;

            auto vertex_buffer = Uuid {};
            auto index_buffer = Uuid {};
            auto index_count = uint32 {0U};
            result = ensure_dynamic_mesh_buffers(
                batch.mesh_data,
                vertex_buffer,
                index_buffer,
                index_count);
            if (!result)
                return result;

            auto instance_buffer = Uuid {};
            result = ensure_dynamic_mesh_instance_buffer(
                mesh_key,
                batch.world_to_clip_transforms,
                instance_buffer);
            if (!result)
                return result;

            out_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = _dynamic_mesh_pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = vertex_buffer,
                            },
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = instance_buffer,
                            },
                        },
                    .index_buffer = index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count =
                                static_cast<uint32>(batch.world_to_clip_transforms.size()),
                        },
                });
        }

        for (const auto& entry : material_draw_batches)
        {
            const uint64 batch_key = entry.first;
            const auto& batch = entry.second;
            if (!batch.mesh_data || batch.model_to_world_transforms.empty())
                continue;

            auto vertex_buffer = Uuid {};
            auto index_buffer = Uuid {};
            auto index_count = uint32 {0U};
            result = ensure_dynamic_mesh_buffers(
                batch.mesh_data,
                vertex_buffer,
                index_buffer,
                index_count);
            if (!result)
                return result;

            auto instance_buffer = Uuid {};
            result = ensure_dynamic_mesh_instance_buffer(
                batch_key,
                batch.model_to_world_transforms,
                instance_buffer);
            if (!result)
                return result;

            out_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.material.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = vertex_buffer,
                            },
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = instance_buffer,
                            },
                        },
                    .index_buffer = index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = view_uniform_buffer,
                            },
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = batch.material.uniform_buffer,
                            },
                        },
                    .textures = batch.material.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count =
                                static_cast<uint32>(batch.model_to_world_transforms.size()),
                        },
                });
        }

        out_fallback_vertices = std::move(fallback_geometry.vertices);
        out_fallback_indices = std::move(fallback_geometry.indices);
        return result;
    }

    Result Rendering::append_static_model_draws(
        const Mat4& view_projection,
        const Uuid& view_uniform_buffer,
        std::vector<GraphicsIndexedDrawCommand>& out_draws)
    {
        auto result = Result {};
        auto material_draw_batches = std::unordered_map<uint64, StaticMaterialDrawBatch> {};
        const auto ensure_material_uniform =
            [this](const uint64 material_key, const void* data, const uint64 data_size, Uuid& out)
        {
            return ensure_material_uniform_buffer(material_key, data, data_size, out);
        };

        _entity_registry.get().for_each_with<StaticMesh, Transform>(
            [this,
             &out_draws,
             &material_draw_batches,
             &ensure_material_uniform,
             &result,
             &view_projection,
             &view_uniform_buffer](Entity& entity)
            {
                if (!result)
                    return;

                const auto& static_mesh = entity.get_component<StaticMesh>();
                if (!static_mesh.handle.is_valid())
                    return;

                auto model_resource = GraphicsModelResource {};
                result = _resource_manager->load_model(static_mesh.handle, model_resource);
                if (!result)
                    return;

                const Mat4 model_to_world =
                    build_transform_matrix(get_world_space_transform(entity));
                const bool has_material = entity.has_component<MaterialInstance>();
                auto material = MaterialDrawState {};
                auto transform_buffer = Uuid {};
                if (has_material)
                {
                    result = resolve_material_draw_state(
                        entity.get_component<MaterialInstance>(),
                        *_resource_manager,
                        ensure_material_uniform,
                        material);
                    if (!result)
                        return;
                }
                else
                {
                    const Mat4 world_to_clip = view_projection * model_to_world;
                    result = ensure_model_transform_buffer(
                        entity.get_id(),
                        world_to_clip,
                        transform_buffer);
                    if (!result)
                        return;
                }

                for (const auto& mesh : model_resource.meshes)
                {
                    if (has_material)
                    {
                        const uint64 batch_key = make_static_material_batch_key(
                            mesh.vertex_buffer,
                            mesh.index_buffer,
                            mesh.index_count,
                            material.key);
                        auto& batch = material_draw_batches[batch_key];
                        if (!batch.vertex_buffer.is_valid())
                        {
                            batch.vertex_buffer = mesh.vertex_buffer;
                            batch.index_buffer = mesh.index_buffer;
                            batch.index_count = mesh.index_count;
                            batch.material = material;
                        }
                        batch.model_to_world_transforms.push_back(model_to_world);
                        continue;
                    }

                    auto command = GraphicsIndexedDrawCommand {
                        .pipeline = _model_pipeline,
                        .vertex_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = mesh.vertex_buffer,
                                },
                            },
                        .index_buffer = mesh.index_buffer,
                        .index_type = GraphicsIndexType::UINT32,
                        .uniform_buffers =
                            {
                                GraphicsResourceBinding {
                                    .slot = 0U,
                                    .resource = transform_buffer,
                                },
                            },
                        .draw =
                            GraphicsDrawIndexedDesc {
                                .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                                .index_type = GraphicsIndexType::UINT32,
                                .index_count = mesh.index_count,
                                .instance_count = 1U,
                            },
                    };

                    out_draws.push_back(std::move(command));
                }
            });

        if (!result)
            return result;

        for (const auto& entry : material_draw_batches)
        {
            const uint64 batch_key = entry.first;
            const auto& batch = entry.second;
            if (!batch.vertex_buffer.is_valid() || !batch.index_buffer.is_valid()
                || batch.model_to_world_transforms.empty())
                continue;

            auto instance_buffer = Uuid {};
            result = ensure_dynamic_mesh_instance_buffer(
                batch_key,
                batch.model_to_world_transforms,
                instance_buffer);
            if (!result)
                return result;

            out_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.material.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = batch.vertex_buffer,
                            },
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = instance_buffer,
                            },
                        },
                    .index_buffer = batch.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = view_uniform_buffer,
                            },
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = batch.material.uniform_buffer,
                            },
                        },
                    .textures = batch.material.textures,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = batch.index_count,
                            .instance_count =
                                static_cast<uint32>(batch.model_to_world_transforms.size()),
                        },
                });
        }

        return result;
    }

    void Rendering::release_resources()
    {
        if (!_geometry_index_buffer.is_valid() && !_geometry_vertex_buffer.is_valid()
            && !_geometry_pipeline.is_valid() && !_dynamic_mesh_pipeline.is_valid()
            && !_model_pipeline.is_valid() && !_view_uniform_buffer.is_valid()
            && _model_transform_buffers.empty() && _dynamic_mesh_vertex_buffers.empty()
            && _dynamic_mesh_index_buffers.empty() && _dynamic_mesh_instance_buffers.empty()
            && _material_uniform_buffers.empty())
            return;

        auto& backend = _backend.get();
        backend.wait_for_idle();

        if (_geometry_index_buffer.is_valid())
            backend.unload(_geometry_index_buffer);
        if (_geometry_vertex_buffer.is_valid())
            backend.unload(_geometry_vertex_buffer);
        if (_geometry_pipeline.is_valid())
            backend.unload(_geometry_pipeline);
        if (_dynamic_mesh_pipeline.is_valid())
            backend.unload(_dynamic_mesh_pipeline);
        if (_model_pipeline.is_valid())
            backend.unload(_model_pipeline);
        for (const auto& entry : _model_transform_buffers)
            backend.unload(entry.second);
        for (const auto& entry : _dynamic_mesh_vertex_buffers)
            backend.unload(entry.second);
        for (const auto& entry : _dynamic_mesh_index_buffers)
            backend.unload(entry.second);
        for (const auto& entry : _dynamic_mesh_instance_buffers)
            backend.unload(entry.second);
        for (const auto& entry : _material_uniform_buffers)
            backend.unload(entry.second);
        if (_view_uniform_buffer.is_valid())
            backend.unload(_view_uniform_buffer);

        _geometry_index_buffer = {};
        _geometry_vertex_buffer = {};
        _geometry_pipeline = {};
        _dynamic_mesh_pipeline = {};
        _model_pipeline = {};
        _geometry_index_buffer_size = 0U;
        _geometry_vertex_buffer_size = 0U;
        _dynamic_mesh_index_buffers.clear();
        _dynamic_mesh_index_counts.clear();
        _dynamic_mesh_instance_buffers.clear();
        _dynamic_mesh_instance_buffer_sizes.clear();
        _dynamic_mesh_last_access_frames.clear();
        _dynamic_mesh_sources.clear();
        _dynamic_mesh_vertex_buffers.clear();
        _material_uniform_buffers.clear();
        _material_uniform_last_access_frames.clear();
        _model_transform_last_access_frames.clear();
        _model_transform_buffers.clear();
        _view_uniform_buffer = {};

        _resource_manager->unload_all();
    }

    void Rendering::unload_stale_dynamic_mesh_buffers()
    {
        constexpr uint64 unused_frame_limit = 3U;
        auto expired_meshes = std::vector<uint64> {};
        for (const auto& entry : _dynamic_mesh_last_access_frames)
        {
            if (_render_frame < entry.second)
                continue;
            if (_render_frame - entry.second < unused_frame_limit)
                continue;

            expired_meshes.push_back(entry.first);
        }

        auto& backend = _backend.get();
        for (const uint64 mesh_key : expired_meshes)
        {
            if (const auto vertex_iterator = _dynamic_mesh_vertex_buffers.find(mesh_key);
                vertex_iterator != _dynamic_mesh_vertex_buffers.end())
            {
                backend.unload(vertex_iterator->second);
                _dynamic_mesh_vertex_buffers.erase(vertex_iterator);
            }

            if (const auto index_iterator = _dynamic_mesh_index_buffers.find(mesh_key);
                index_iterator != _dynamic_mesh_index_buffers.end())
            {
                backend.unload(index_iterator->second);
                _dynamic_mesh_index_buffers.erase(index_iterator);
            }

            if (const auto instance_iterator = _dynamic_mesh_instance_buffers.find(mesh_key);
                instance_iterator != _dynamic_mesh_instance_buffers.end())
            {
                backend.unload(instance_iterator->second);
                _dynamic_mesh_instance_buffers.erase(instance_iterator);
            }

            _dynamic_mesh_index_counts.erase(mesh_key);
            _dynamic_mesh_instance_buffer_sizes.erase(mesh_key);
            _dynamic_mesh_last_access_frames.erase(mesh_key);
            _dynamic_mesh_sources.erase(mesh_key);
        }
    }

    void Rendering::unload_stale_material_uniform_buffers()
    {
        constexpr uint64 unused_frame_limit = 3U;
        auto expired_materials = std::vector<uint64> {};
        for (const auto& entry : _material_uniform_last_access_frames)
        {
            if (_render_frame < entry.second)
                continue;
            if (_render_frame - entry.second < unused_frame_limit)
                continue;

            expired_materials.push_back(entry.first);
        }

        auto& backend = _backend.get();
        for (const uint64 material_key : expired_materials)
        {
            if (const auto uniform_iterator = _material_uniform_buffers.find(material_key);
                uniform_iterator != _material_uniform_buffers.end())
            {
                backend.unload(uniform_iterator->second);
                _material_uniform_buffers.erase(uniform_iterator);
            }

            _material_uniform_last_access_frames.erase(material_key);
        }
    }

    void Rendering::unload_stale_model_transform_buffers()
    {
        constexpr uint64 unused_frame_limit = 3U;
        auto expired_entities = std::vector<Uuid> {};
        for (const auto& entry : _model_transform_last_access_frames)
        {
            if (_render_frame < entry.second)
                continue;
            if (_render_frame - entry.second < unused_frame_limit)
                continue;

            expired_entities.push_back(entry.first);
        }

        auto& backend = _backend.get();
        for (const Uuid entity_id : expired_entities)
        {
            if (const auto transform_iterator = _model_transform_buffers.find(entity_id);
                transform_iterator != _model_transform_buffers.end())
            {
                backend.unload(transform_iterator->second);
                _model_transform_buffers.erase(transform_iterator);
            }

            _model_transform_last_access_frames.erase(entity_id);
        }
    }

    void Rendering::setup_geometry_pass(
        const uint32 index_count,
        std::vector<GraphicsIndexedDrawCommand> static_draws)
    {
        if (!_pipeline)
            return;

        _pipeline->clear();
        auto geometry_pass = GraphicsRenderPass {
            .pass =
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Geometry Pass",
                },
        };

        if (index_count > 0U && _geometry_pipeline.is_valid() && _geometry_vertex_buffer.is_valid()
            && _geometry_index_buffer.is_valid())
        {
            geometry_pass.indexed_draws.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = _geometry_pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {
                                .slot = 0U,
                                .resource = _geometry_vertex_buffer,
                            },
                        },
                    .index_buffer = _geometry_index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count = 1U,
                        },
                });
        }

        for (auto& draw : static_draws)
            geometry_pass.indexed_draws.push_back(std::move(draw));

        _pipeline->add_pass_operation(std::move(geometry_pass));
    }
}
