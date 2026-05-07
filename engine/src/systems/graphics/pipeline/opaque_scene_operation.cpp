#include "tbx/systems/graphics/pipeline/opaque_scene_operation.h"
#include "render_material_helpers.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/entity.h"
#include "tbx/systems/graphics/camera.h"
#include "tbx/systems/graphics/material.h"
#include "tbx/systems/graphics/mesh.h"
#include "tbx/systems/graphics/model.h"
#include "tbx/systems/graphics/pipeline/render_frame_context.h"
#include "tbx/systems/graphics/shader.h"
#include "tbx/systems/math/matrices.h"
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tbx
{
    // ---------------------------------------------------------------------------
    // CPU-side geometry clipping (fallback for non-standard mesh strides)
    // ---------------------------------------------------------------------------

    namespace
    {
        struct GeometryBuildResult
        {
            std::vector<float> vertices = {};
            std::vector<uint32> indices = {};
        };

        float get_clip_plane_distance(const Vec4& position, const uint32 plane_index)
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

        Vec4 interpolate_clip_position(
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

        std::vector<Vec4> clip_polygon_against_plane(
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

        void append_projected_vertex(const Vec4& clip_position, GeometryBuildResult& geometry)
        {
            const float inverse_w =
                std::abs(clip_position.w) <= 0.000001F ? 1.0F : 1.0F / clip_position.w;
            geometry.vertices.push_back(clip_position.x * inverse_w);
            geometry.vertices.push_back(clip_position.y * inverse_w);
            geometry.vertices.push_back(clip_position.z * inverse_w);
        }

        void append_clipped_triangle(
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

        uint32 get_vertex_stride_float_count(const Mesh& mesh)
        {
            const uint32 stride_bytes = mesh.vertices.layout.stride;
            return stride_bytes == 0U ? 16U : stride_bytes / static_cast<uint32>(sizeof(float));
        }

        bool can_render_mesh_directly(const Mesh& mesh)
        {
            constexpr uint32 model_pipeline_stride = 16U;
            return !mesh.vertices.empty() && !mesh.indices.empty()
                   && get_vertex_stride_float_count(mesh) == model_pipeline_stride;
        }

        void append_mesh_geometry(
            const Mesh& mesh,
            const Mat4& world_to_clip,
            GeometryBuildResult& geometry)
        {
            if (mesh.vertices.empty() || mesh.indices.empty())
                return;

            const uint32 stride = get_vertex_stride_float_count(mesh);
            if (stride < 3U)
                return;

            const uint32 source_vertex_count =
                static_cast<uint32>(mesh.vertices.size() / stride);
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

            for (uint32 index_offset = 0U; index_offset + 2U < mesh.indices.size();
                 index_offset += 3U)
            {
                const uint32 i0 = mesh.indices[index_offset];
                const uint32 i1 = mesh.indices[index_offset + 1U];
                const uint32 i2 = mesh.indices[index_offset + 2U];
                if (i0 >= source_vertex_count || i1 >= source_vertex_count
                    || i2 >= source_vertex_count)
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

        Shader make_fallback_shader()
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

        GraphicsPipelineDesc make_fallback_pipeline_desc()
        {
            return GraphicsPipelineDesc {
                .shader = make_fallback_shader(),
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

        uint64 make_mesh_cache_key(const std::shared_ptr<Mesh>& mesh_data)
        {
            return reinterpret_cast<uint64>(mesh_data.get());
        }

        uint64 make_dynamic_batch_key(const uint64 mesh_key, const uint64 material_key)
        {
            return hash_value(material_key, hash_value(mesh_key, 14695981039346656037ULL));
        }

        uint64 make_static_batch_key(
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
    }

    // ---------------------------------------------------------------------------
    // OpaqueSceneOperation implementation
    // ---------------------------------------------------------------------------

    Result OpaqueSceneOperation::ensure_material_uniform_buffer(
        IGraphicsBackend& backend,
        const uint64 material_key,
        const void* data,
        const uint64 data_size,
        Uuid& out_buffer)
    {
        out_buffer = {};
        if (material_key == 0U || data == nullptr || data_size == 0U)
            return Result(false, "OpaqueSceneOperation: invalid material uniform data.");

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

    Result OpaqueSceneOperation::ensure_dynamic_mesh_buffers(
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
            return Result(false, "OpaqueSceneOperation: mesh is not directly renderable.");

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
        _mesh_last_access[mesh_key] = 0U;

        out_vertex_buffer = vertex_buffer;
        out_index_buffer = index_buffer;
        out_index_count = static_cast<uint32>(mesh->indices.size());
        return {};
    }

    Result OpaqueSceneOperation::ensure_instance_buffer(
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

    Result OpaqueSceneOperation::ensure_fallback_pipeline(IGraphicsBackend& backend)
    {
        if (_fallback_pipeline.is_valid())
            return {};
        return backend.upload_pipeline(make_fallback_pipeline_desc(), _fallback_pipeline);
    }

    Result OpaqueSceneOperation::ensure_fallback_geometry_buffers(
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
        else if (const auto result = backend.update_buffer(
                     _fallback_vertex_buffer,
                     vertices.data(),
                     vertex_size,
                     0U);
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

    void OpaqueSceneOperation::evict_stale_resources(
        IGraphicsBackend& backend,
        const uint64 frame_index)
    {
        constexpr uint64 unused_frame_limit = 3U;

        // Evict stale instance buffers (keyed by batch hash)
        auto expired_instances = std::vector<uint64> {};
        for (const auto& [key, last_access] : _instance_last_access)
        {
            if (frame_index >= last_access && frame_index - last_access >= unused_frame_limit)
                expired_instances.push_back(key);
        }
        for (const uint64 key : expired_instances)
        {
            if (const auto it = _instance_buffers.find(key); it != _instance_buffers.end())
            {
                backend.unload(it->second);
                _instance_buffers.erase(it);
            }
            _instance_buffer_sizes.erase(key);
            _instance_last_access.erase(key);
        }

        // Evict stale dynamic mesh geometry (keyed by Mesh* address)
        auto expired_meshes = std::vector<uint64> {};
        for (const auto& [key, last_access] : _mesh_last_access)
        {
            if (frame_index >= last_access && frame_index - last_access >= unused_frame_limit)
                expired_meshes.push_back(key);
        }
        for (const uint64 key : expired_meshes)
        {
            if (const auto it = _mesh_vertex_buffers.find(key); it != _mesh_vertex_buffers.end())
            {
                backend.unload(it->second);
                _mesh_vertex_buffers.erase(it);
            }
            if (const auto it = _mesh_index_buffers.find(key); it != _mesh_index_buffers.end())
            {
                backend.unload(it->second);
                _mesh_index_buffers.erase(it);
            }
            _mesh_index_counts.erase(key);
            _mesh_sources.erase(key);
            _mesh_last_access.erase(key);
        }

        // Evict stale material uniform buffers (keyed by material content hash)
        auto expired_materials = std::vector<uint64> {};
        for (const auto& [key, last_access] : _material_uniform_last_access)
        {
            if (frame_index >= last_access && frame_index - last_access >= unused_frame_limit)
                expired_materials.push_back(key);
        }
        for (const uint64 key : expired_materials)
        {
            if (const auto it = _material_uniform_buffers.find(key);
                it != _material_uniform_buffers.end())
            {
                backend.unload(it->second);
                _material_uniform_buffers.erase(it);
            }
            _material_uniform_last_access.erase(key);
        }
    }

    Result OpaqueSceneOperation::prepare(RenderFrameContext& context)
    {
        _draw_commands.clear();
        _clear_flags = context.has_skybox ? GraphicsClearFlags::DEPTH
                                          : GraphicsClearFlags::COLOR_DEPTH;

        auto& backend = context.backend.get();
        auto& resource_manager = context.resource_manager.get();
        auto& registry = context.entity_registry.get();
        const uint64 frame_index = context.frame_index;
        const Uuid view_uniform_buffer = context.view_uniform_buffer;
        const auto default_material = MaterialInstance(Handle("Materials/Flat.mat"));

        // -------------------------------------------------------------------
        // Touch resource manager to update LRU access — mark all used materials
        // -------------------------------------------------------------------
        auto touched_materials = std::unordered_set<Handle> {};
        auto touched_textures = std::unordered_set<Handle> {};

        const auto touch_material = [&](const MaterialInstance& material)
        {
            const Handle& handle = material.get_handle();
            if (handle.is_valid() && touched_materials.insert(handle).second)
            {
                auto unused = Uuid {};
                resource_manager.load_material(handle, unused);
            }
            for (const auto& override : material.texture_overrides)
            {
                if (!override.texture.is_valid())
                    continue;
                if (!touched_textures.insert(override.texture).second)
                    continue;
                auto unused = Uuid {};
                resource_manager.load_texture(override.texture, unused);
            }
        };

        // -------------------------------------------------------------------
        // Resolve material draw state — uses the shared ensure_material_uniform_buffer
        // but records the access for eviction purposes
        // -------------------------------------------------------------------
        const auto resolve_and_store_material =
            [&](const MaterialInstance& material,
                Uuid& out_pipeline,
                uint64& out_material_key,
                Uuid& out_uniform_buffer,
                std::vector<GraphicsResourceBinding>& out_textures) -> Result
        {
            auto resolved = ResolvedMaterial {};
            if (const auto result = resolve_material(material, resource_manager, resolved);
                !result)
                return result;

            Uuid uniform_buffer = {};
            if (const auto result = ensure_material_uniform_buffer(
                    backend,
                    resolved.uniform_key,
                    resolved.uniform_data.data(),
                    resolved.uniform_data.byte_size(),
                    uniform_buffer);
                !result)
                return result;

            _material_uniform_last_access[resolved.uniform_key] = frame_index;

            out_pipeline = resolved.pipeline;
            out_material_key = resolved.uniform_key;
            out_uniform_buffer = uniform_buffer;
            out_textures = std::move(resolved.textures);
            return {};
        };

        // -------------------------------------------------------------------
        // Dynamic mesh pass — batch by (mesh × material)
        // -------------------------------------------------------------------

        auto fallback_geometry = GeometryBuildResult {};
        auto dynamic_batches = std::unordered_map<uint64, DynamicBatch> {};
        auto prepare_result = Result {};

        registry.for_each_with<DynamicMesh, Transform>(
            [&](Entity& entity)
            {
                if (!prepare_result)
                    return;

                const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
                const std::shared_ptr<Mesh>& mesh_data = dynamic_mesh.data;
                if (!mesh_data)
                    return;

                const Mat4 model_to_world =
                    build_transform_matrix(get_world_space_transform(entity));

                if (!can_render_mesh_directly(*mesh_data))
                {
                    if (entity.has_component<MaterialInstance>())
                        touch_material(entity.get_component<MaterialInstance>());
                    append_mesh_geometry(
                        *mesh_data,
                        context.view_projection * model_to_world,
                        fallback_geometry);
                    return;
                }

                const MaterialInstance& material_instance = entity.has_component<MaterialInstance>()
                    ? entity.get_component<MaterialInstance>()
                    : default_material;

                Uuid pipeline = {};
                uint64 material_key = 0U;
                Uuid uniform_buffer = {};
                auto textures = std::vector<GraphicsResourceBinding> {};
                prepare_result = resolve_and_store_material(
                    material_instance,
                    pipeline,
                    material_key,
                    uniform_buffer,
                    textures);
                if (!prepare_result)
                    return;

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
            });

        if (!prepare_result)
            return prepare_result;

        // -------------------------------------------------------------------
        // Static model pass — batch by (mesh × material)
        // -------------------------------------------------------------------

        auto static_batches = std::unordered_map<uint64, StaticBatch> {};

        registry.for_each_with<StaticMesh, Transform>(
            [&](Entity& entity)
            {
                if (!prepare_result)
                    return;

                const auto& static_mesh = entity.get_component<StaticMesh>();
                if (!static_mesh.handle.is_valid())
                    return;

                auto model_resource = GraphicsModelResource {};
                prepare_result = resource_manager.load_model(static_mesh.handle, model_resource);
                if (!prepare_result)
                    return;

                const Mat4 model_to_world =
                    build_transform_matrix(get_world_space_transform(entity));
                const MaterialInstance& material_instance = entity.has_component<MaterialInstance>()
                    ? entity.get_component<MaterialInstance>()
                    : default_material;

                Uuid pipeline = {};
                uint64 material_key = 0U;
                Uuid uniform_buffer = {};
                auto textures = std::vector<GraphicsResourceBinding> {};
                prepare_result = resolve_and_store_material(
                    material_instance,
                    pipeline,
                    material_key,
                    uniform_buffer,
                    textures);
                if (!prepare_result)
                    return;

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
            });

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

            const uint64 mesh_key = make_mesh_cache_key(batch.mesh_data);
            _mesh_last_access[mesh_key] = frame_index;

            Uuid instance_buffer = {};
            if (const auto result = ensure_instance_buffer(
                    backend,
                    batch_key,
                    batch.transforms,
                    instance_buffer);
                !result)
                return result;

            _instance_last_access[batch_key] = frame_index;

            _draw_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = vertex_buffer},
                            GraphicsResourceBinding {.slot = 1U, .resource = instance_buffer},
                        },
                    .index_buffer = index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = view_uniform_buffer},
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = batch.material_uniform_buffer},
                        },
                    .textures = std::move(batch.textures),
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = index_count,
                            .instance_count =
                                static_cast<uint32>(batch.transforms.size()),
                        },
                });
        }

        for (auto& [batch_key, batch] : static_batches)
        {
            if (!batch.vertex_buffer.is_valid() || batch.transforms.empty())
                continue;

            Uuid instance_buffer = {};
            if (const auto result = ensure_instance_buffer(
                    backend,
                    batch_key,
                    batch.transforms,
                    instance_buffer);
                !result)
                return result;

            _instance_last_access[batch_key] = frame_index;

            _draw_commands.push_back(
                GraphicsIndexedDrawCommand {
                    .pipeline = batch.pipeline,
                    .vertex_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = batch.vertex_buffer},
                            GraphicsResourceBinding {.slot = 1U, .resource = instance_buffer},
                        },
                    .index_buffer = batch.index_buffer,
                    .index_type = GraphicsIndexType::UINT32,
                    .uniform_buffers =
                        {
                            GraphicsResourceBinding {.slot = 0U, .resource = view_uniform_buffer},
                            GraphicsResourceBinding {
                                .slot = 1U,
                                .resource = batch.material_uniform_buffer},
                        },
                    .textures = std::move(batch.textures),
                    .draw =
                        GraphicsDrawIndexedDesc {
                            .primitive_type = GraphicsPrimitiveType::TRIANGLES,
                            .index_type = GraphicsIndexType::UINT32,
                            .index_count = batch.index_count,
                            .instance_count =
                                static_cast<uint32>(batch.transforms.size()),
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

            _draw_commands.push_back(
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
                            .index_count =
                                static_cast<uint32>(fallback_geometry.indices.size()),
                            .instance_count = 1U,
                        },
                });
        }

        evict_stale_resources(backend, frame_index);
        return {};
    }

    Result OpaqueSceneOperation::execute(
        IGraphicsBackend& backend,
        const CancellationToken& token)
    {
        if (token && token.is_cancelled())
            return Result(false, "OpaqueSceneOperation cancelled.");

        if (const auto result = backend.begin_pass(
                GraphicsPassDesc {
                    .clear_color = Color::BLACK,
                    .clear_depth = 1.0F,
                    .clear_stencil = 0U,
                    .clear_flags = _clear_flags,
                    .debug_name = "Toybox Opaque Scene Pass",
                });
            !result)
            return result;

        for (const auto& draw : _draw_commands)
        {
            if (token && token.is_cancelled())
                return backend.end_pass(),
                       Result(false, "OpaqueSceneOperation cancelled mid-pass.");

            if (const auto result = execute_indexed_draw(backend, draw); !result)
                return backend.end_pass(), result;
        }

        return backend.end_pass();
    }

    void OpaqueSceneOperation::release(IGraphicsBackend& backend)
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
        _mesh_last_access.clear();
        _instance_buffers.clear();
        _instance_buffer_sizes.clear();
        _instance_last_access.clear();
        _material_uniform_buffers.clear();
        _material_uniform_last_access.clear();
        _fallback_pipeline = {};
        _fallback_vertex_buffer = {};
        _fallback_index_buffer = {};
        _fallback_vertex_buffer_size = 0U;
        _fallback_index_buffer_size = 0U;
        _draw_commands.clear();
    }
}
