#include "tbx/systems/graphics/pipeline/commands/opaque_pass_operation.h"
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
    OpaquePassOperation::OpaquePassOperation(
        std::weak_ptr<IGraphicsBackend> backend,
        GraphicsResourceManager& resource_manager)
        : _backend(std::move(backend))
        , _resource_manager(resource_manager)
    {
    }

    RenderOperationDebugInfo OpaquePassOperation::get_debug_info() const
    {
        auto debug_info = RenderOperationDebugInfo();
        debug_info.debug_name = "Toybox Opaque Pass Operation";
        debug_info.category = "Scene Rendering";
        return debug_info;
    }

    std::vector<GraphicsResourceBinding> OpaquePassOperation::make_scene_uniform_bindings(
        const Uuid view_uniform_buffer,
        const Uuid material_uniform_buffer)
    {
        return std::vector<GraphicsResourceBinding> {
            GraphicsResourceBinding {.slot = 0U, .resource = view_uniform_buffer},
            GraphicsResourceBinding {.slot = 2U, .resource = material_uniform_buffer},
        };
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

    Result OpaquePassOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (material_key == 0U || data == nullptr || data_size == 0U)
            return Result(false, "OpaquePassOperation: invalid material uniform data.");

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

    Result OpaquePassOperation::ensure_dynamic_mesh_buffers(
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
            return Result(false, "OpaquePassOperation: mesh is not directly renderable.");

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
                    .debug_name = "Toybox Dynamic Mesh Vertices",
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
                    .debug_name = "Toybox Dynamic Mesh Indices",
                },
                mesh->indices.data(),
                index_size,
                index_buffer);
            !result)
        {
            _resource_manager.get().unload(vertex_buffer);
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

    Result OpaquePassOperation::ensure_instance_buffer(
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
                _resource_manager.get().update(buffer_it->second, transforms.data(), data_size, 0U);
            !result)
            return result;

        out_buffer = buffer_it->second;
        return {};
    }

    Result OpaquePassOperation::ensure_fallback_pipeline(IGraphicsBackend& backend)
    {
        if (_fallback_pipeline.is_valid())
        {
            _resource_manager.get().update(_fallback_pipeline);
            return {};
        }
        return _resource_manager.get().upload(make_fallback_pipeline_desc(), _fallback_pipeline);
    }

    Result OpaquePassOperation::ensure_fallback_geometry_buffers(
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
                _resource_manager.get().unload(_fallback_vertex_buffer);

            if (const auto result = _resource_manager.get().upload(
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
        else if (
            const auto result =
                _resource_manager.get()
                    .update(_fallback_vertex_buffer, vertices.data(), vertex_size, 0U);
            !result)
        {
            return result;
        }

        if (!_fallback_index_buffer.is_valid() || index_size > _fallback_index_buffer_size)
        {
            if (_fallback_index_buffer.is_valid())
                _resource_manager.get().unload(_fallback_index_buffer);

            if (const auto result = _resource_manager.get().upload(
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
        else if (
            const auto result = _resource_manager.get()
                                    .update(_fallback_index_buffer, indices.data(), index_size, 0U);
            !result)
        {
            return result;
        }

        return {};
    }
    Result OpaquePassOperation::prepare(RenderData& render_data)
    {
        auto& frame_data = render_data;
        render_data.opaque_commands.clear();
        auto backend_ptr = _backend.lock();
        if (!backend_ptr)
        {
            return Result(
                false,
                "OpaquePassOperation requires IGraphicsBackend service.");
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
                    resource_manager.upload(material, material_resource);
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
            prepare_result = resource_manager.upload(
                renderable.static_mesh,
                ModelLoadParameters {},
                model_resource);
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

    Result OpaquePassOperation::execute(
        IGraphicsBackend& backend,
        RenderData& render_data,
        const CancellationToken& token)
    {
        return execute_draw_list(
            backend,
            render_data,
            render_data.opaque_commands,
            make_scene_pass_desc(
                render_data,
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = render_data.has_skybox ? GraphicsClearFlags::DEPTH
                                                          : GraphicsClearFlags::COLOR_DEPTH,
                    .debug_name = "Toybox Opaque Scene Pass",
                }),
            token,
            "OpaquePassOperation");
    }
}
