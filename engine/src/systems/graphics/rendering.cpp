#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/math/matrices.h"
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace tbx
{
    struct GeometryBuildResult
    {
        std::vector<float> vertices = {};
        std::vector<uint32> indices = {};
    };

    struct RenderCamera
    {
        Camera camera = {};
        Transform transform = {};
    };

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

    static uint32 get_vertex_stride_float_count(const Mesh& mesh)
    {
        const uint32 stride_bytes = mesh.vertices.layout.stride;
        if (stride_bytes == 0U)
            return 16U;

        return stride_bytes / static_cast<uint32>(sizeof(float));
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

    static RenderCamera find_render_camera(EntityRegistry& entity_registry, Size resolution)
    {
        auto render_camera = RenderCamera {
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

    static GeometryBuildResult build_geometry(
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        RenderCamera render_camera)
    {
        auto geometry = GeometryBuildResult {};
        const Mat4 view_projection = render_camera.camera.get_view_projection_matrix(
            render_camera.transform.position,
            render_camera.transform.rotation);

        entity_registry.for_each_with<DynamicMesh, Transform>(
            [&geometry, &view_projection](Entity& entity)
            {
                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                if (!dynamic_mesh.data)
                    return;

                const Mat4 world_to_clip =
                    view_projection * build_transform_matrix(get_world_space_transform(entity));
                append_mesh_geometry(*dynamic_mesh.data, world_to_clip, geometry);
            });

        entity_registry.for_each_with<StaticMesh, Transform>(
            [&geometry, &view_projection, &asset_manager](Entity& entity)
            {
                const auto& static_mesh = entity.get_component<StaticMesh>();
                if (!static_mesh.handle.is_valid())
                    return;

                const std::shared_ptr<Model> model = asset_manager.load<Model>(static_mesh.handle);
                if (!model)
                    return;

                const Mat4 entity_to_clip =
                    view_projection * build_transform_matrix(get_world_space_transform(entity));
                for (const auto& mesh : model->meshes)
                    append_mesh_geometry(mesh, entity_to_clip, geometry);
            });

        return geometry;
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
        , _asset_manager(asset_manager)
        , _window_manager(window_manager)
        , _output_window(std::move(output_window))
        , _requested_resolution(settings.resolution.value)
        , _pipeline(std::make_unique<GraphicsRenderPipeline>(backend))
    {
        _initialization_result = backend.initialize(settings);
        setup_geometry_pass(0U);
    }

    Rendering::~Rendering() noexcept
    {
        release_resources();
    }

    Result Rendering::render()
    {
        if (!_initialization_result)
            return _initialization_result;
        if (!_output_window.is_valid() || !_window_manager.get().is_open(_output_window))
            return {};

        if (const auto result = begin_frame_and_view(); !result)
            return result;

        const auto finish_after_failure = [this](const Result& failure)
        {
            auto& backend = _backend.get();
            backend.end_view();
            backend.end_frame();
            return failure;
        };

        if (const auto result = ensure_geometry_pipeline(); !result)
            return finish_after_failure(result);

        const Size resolution = get_render_resolution();
        const RenderCamera render_camera = find_render_camera(_entity_registry.get(), resolution);
        const GeometryBuildResult geometry =
            build_geometry(_entity_registry.get(), _asset_manager.get(), render_camera);

        if (const auto result = ensure_geometry_buffers(geometry.vertices, geometry.indices);
            !result)
            return finish_after_failure(result);

        setup_geometry_pass(static_cast<uint32>(geometry.indices.size()));
        if (const auto result = _pipeline->execute(); !result)
            return finish_after_failure(result);

        return end_view_and_frame();
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

        const RenderCamera render_camera = find_render_camera(_entity_registry.get(), resolution);
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

    Size Rendering::get_render_resolution() const
    {
        if (_requested_resolution.width > 0U && _requested_resolution.height > 0U)
            return _requested_resolution;

        return _window_manager.get().get_size(_output_window);
    }

    void Rendering::release_resources()
    {
        if (!_geometry_index_buffer.is_valid() && !_geometry_vertex_buffer.is_valid()
            && !_geometry_pipeline.is_valid())
            return;

        auto& backend = _backend.get();
        backend.wait_for_idle();

        if (_geometry_index_buffer.is_valid())
            backend.unload(_geometry_index_buffer);
        if (_geometry_vertex_buffer.is_valid())
            backend.unload(_geometry_vertex_buffer);
        if (_geometry_pipeline.is_valid())
            backend.unload(_geometry_pipeline);

        _geometry_index_buffer = {};
        _geometry_vertex_buffer = {};
        _geometry_pipeline = {};
        _geometry_index_buffer_size = 0U;
        _geometry_vertex_buffer_size = 0U;
    }

    void Rendering::setup_geometry_pass(const uint32 index_count)
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

        _pipeline->add_pass_operation(std::move(geometry_pass));
    }
}
