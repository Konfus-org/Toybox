#include "tbx/graphics/render_resources.h"
#include "tbx/assets/manager.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/render_pipeline.h"
#include "tbx/graphics/texture.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/model.h"
#include "tbx/math/matrices.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace tbx
{
    constexpr uint32 DynamicMeshSalt = 0xD1A60001U;
    constexpr uint32 StaticMeshSalt = 0x57A71C01U;
    constexpr uint32 RenderTextureSalt = 0x7EAE1001U;
    constexpr uint32 TextureSalt = 0x7EAE0001U;
    constexpr uint32 MaterialProgramSalt = 0xAA110001U;

    struct MeshPartQueueEntry final
    {
        size_t part_index = 0U;
        Mat4 parent_transform = Mat4(1.0F);
    };

    static Uuid make_non_zero_uuid_from_hash(const std::size_t hash_value)
    {
        auto hashed = static_cast<uint32>(hash_value);
        if (hashed == 0U)
            hashed = 1U;
        return Uuid(hashed);
    }

    static Uuid make_typed_key(const Uuid& base_key, const uint32 salt)
    {
        if (!base_key.is_valid())
            return {};

        auto typed_key = Uuid(base_key);
        typed_key.combine(salt);
        if (!typed_key.is_valid())
            return Uuid(1U);
        return typed_key;
    }

    static Uuid make_dynamic_mesh_key(const std::shared_ptr<Mesh>& mesh)
    {
        if (!mesh)
            return {};

        auto* mesh_address = mesh.get();
        const auto hash_value =
            std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(mesh_address));
        return make_typed_key(make_non_zero_uuid_from_hash(hash_value), DynamicMeshSalt);
    }

    static Uuid make_static_mesh_key(const Handle& mesh_handle)
    {
        return make_typed_key(mesh_handle.get_id(), StaticMeshSalt);
    }

    static Uuid make_texture_key(const Handle& texture_handle)
    {
        return make_typed_key(texture_handle.get_id(), TextureSalt);
    }

    static Uuid make_render_texture_key(const Uuid& resource_uuid)
    {
        return make_typed_key(resource_uuid, RenderTextureSalt);
    }

    static Uuid make_material_program_key(const Handle& material_handle)
    {
        return make_typed_key(material_handle.get_id(), MaterialProgramSalt);
    }

    static Handle resolve_asset_handle(const Handle& handle, AssetManager& asset_manager)
    {
        if (handle.get_name().empty() && !handle.get_id().is_valid())
            return {};

        const auto resolved_id = asset_manager.ensure(handle);
        if (resolved_id.is_valid())
            return Handle(handle.get_name(), resolved_id);
        return handle;
    }

    static void append_unique_uuid(std::vector<Uuid>& values, const Uuid& value)
    {
        if (!value.is_valid())
            return;

        for (const auto& existing : values)
        {
            if (existing == value)
                return;
        }

        values.push_back(value);
    }

    static bool try_get_mesh_attribute_offsets(
        const VertexBufferLayout& layout,
        size_t& out_position_offset_floats,
        size_t& out_normal_offset_floats,
        bool& out_has_normal,
        size_t& out_tangent_offset_floats,
        bool& out_has_tangent)
    {
        out_position_offset_floats = 0U;
        out_normal_offset_floats = 0U;
        out_has_normal = false;
        out_tangent_offset_floats = 0U;
        out_has_tangent = false;

        auto vec3_attribute_index = size_t {0U};
        auto has_position = false;
        for (const auto& attribute : layout.elements)
        {
            const auto attribute_offset_floats =
                static_cast<size_t>(attribute.offset) / sizeof(float);
            if (std::holds_alternative<Vec3>(attribute.type))
            {
                if (vec3_attribute_index == 0U)
                {
                    out_position_offset_floats = attribute_offset_floats;
                    has_position = true;
                }
                else if (vec3_attribute_index == 1U)
                {
                    out_normal_offset_floats = attribute_offset_floats;
                    out_has_normal = true;
                }

                vec3_attribute_index += 1U;
            }
            else if (std::holds_alternative<Vec4>(attribute.type) && !out_has_tangent)
            {
                out_tangent_offset_floats = attribute_offset_floats;
                out_has_tangent = true;
            }
        }

        return has_position;
    }

    static bool append_transformed_mesh(
        const Mesh& mesh,
        const Mat4& transform,
        std::vector<float>& out_vertices,
        std::vector<uint32>& out_indices,
        VertexBufferLayout& out_layout,
        bool& out_has_layout)
    {
        const auto& source_vertices = mesh.vertices.vertices;
        const auto& source_layout = mesh.vertices.layout;
        if (source_layout.stride == 0U || (source_layout.stride % sizeof(float)) != 0U)
            return false;

        const auto stride_floats = static_cast<size_t>(source_layout.stride) / sizeof(float);
        if (stride_floats == 0U || (source_vertices.size() % stride_floats) != 0U)
            return false;

        auto position_offset_floats = size_t {0U};
        auto normal_offset_floats = size_t {0U};
        auto has_normal = false;
        auto tangent_offset_floats = size_t {0U};
        auto has_tangent = false;
        if (!try_get_mesh_attribute_offsets(
                source_layout,
                position_offset_floats,
                normal_offset_floats,
                has_normal,
                tangent_offset_floats,
                has_tangent))
            return false;

        if (!out_has_layout)
        {
            out_layout = source_layout;
            out_has_layout = true;
        }
        else if (out_layout.stride != source_layout.stride)
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: skipping mesh append due to mismatched model vertex layouts.");
            return false;
        }

        const auto base_vertex_index = static_cast<uint32>(out_vertices.size() / stride_floats);
        out_vertices.reserve(out_vertices.size() + source_vertices.size());
        const auto transform_matrix = Mat3(transform);
        const auto normal_matrix = inverse_transpose(transform_matrix);
        const auto transform_determinant = glm::determinant(transform_matrix);
        const auto tangent_handedness_sign = transform_determinant < 0.0F ? -1.0F : 1.0F;

        const auto vertex_count = source_vertices.size() / stride_floats;
        for (size_t vertex_index = 0U; vertex_index < vertex_count; ++vertex_index)
        {
            const auto source_base_index = vertex_index * stride_floats;
            out_vertices.insert(
                out_vertices.end(),
                source_vertices.begin() + static_cast<std::ptrdiff_t>(source_base_index),
                source_vertices.begin()
                    + static_cast<std::ptrdiff_t>(source_base_index + stride_floats));

            const auto destination_base_index = out_vertices.size() - stride_floats;
            const auto transformed_position =
                transform
                * Vec4(
                    source_vertices[source_base_index + position_offset_floats],
                    source_vertices[source_base_index + position_offset_floats + 1U],
                    source_vertices[source_base_index + position_offset_floats + 2U],
                    1.0F);
            out_vertices[destination_base_index + position_offset_floats] = transformed_position.x;
            out_vertices[destination_base_index + position_offset_floats + 1U] =
                transformed_position.y;
            out_vertices[destination_base_index + position_offset_floats + 2U] =
                transformed_position.z;

            if (!has_normal)
                continue;

            auto transformed_normal =
                normal_matrix
                * Vec3(
                    source_vertices[source_base_index + normal_offset_floats],
                    source_vertices[source_base_index + normal_offset_floats + 1U],
                    source_vertices[source_base_index + normal_offset_floats + 2U]);
            const auto normal_length_squared =
                (transformed_normal.x * transformed_normal.x)
                + (transformed_normal.y * transformed_normal.y)
                + (transformed_normal.z * transformed_normal.z);
            if (normal_length_squared > 0.000001F)
            {
                const auto inverse_length = 1.0F / std::sqrt(normal_length_squared);
                transformed_normal *= inverse_length;
            }

            out_vertices[destination_base_index + normal_offset_floats] = transformed_normal.x;
            out_vertices[destination_base_index + normal_offset_floats + 1U] =
                transformed_normal.y;
            out_vertices[destination_base_index + normal_offset_floats + 2U] =
                transformed_normal.z;

            if (!has_tangent)
                continue;

            auto transformed_tangent =
                transform_matrix
                * Vec3(
                    source_vertices[source_base_index + tangent_offset_floats],
                    source_vertices[source_base_index + tangent_offset_floats + 1U],
                    source_vertices[source_base_index + tangent_offset_floats + 2U]);
            transformed_tangent -= transformed_normal * dot(transformed_tangent, transformed_normal);
            const auto tangent_length_squared =
                (transformed_tangent.x * transformed_tangent.x)
                + (transformed_tangent.y * transformed_tangent.y)
                + (transformed_tangent.z * transformed_tangent.z);
            if (tangent_length_squared > 0.000001F)
            {
                const auto inverse_tangent_length = 1.0F / std::sqrt(tangent_length_squared);
                transformed_tangent *= inverse_tangent_length;
            }
            else
            {
                transformed_tangent = Vec3(1.0F, 0.0F, 0.0F);
            }

            out_vertices[destination_base_index + tangent_offset_floats] = transformed_tangent.x;
            out_vertices[destination_base_index + tangent_offset_floats + 1U] =
                transformed_tangent.y;
            out_vertices[destination_base_index + tangent_offset_floats + 2U] =
                transformed_tangent.z;
            out_vertices[destination_base_index + tangent_offset_floats + 3U] =
                source_vertices[source_base_index + tangent_offset_floats + 3U]
                * tangent_handedness_sign;
        }

        out_indices.reserve(out_indices.size() + mesh.indices.size());
        for (const auto index : mesh.indices)
            out_indices.push_back(base_vertex_index + index);

        return true;
    }

    static bool try_build_static_mesh_render_geometry(const Model& model, Mesh& out_mesh)
    {
        if (model.meshes.empty())
            return false;

        auto flattened_vertices = std::vector<float> {};
        auto flattened_indices = std::vector<uint32> {};
        auto flattened_layout = VertexBufferLayout {};
        auto has_layout = false;
        auto appended_any_mesh = false;

        if (model.parts.empty())
        {
            for (const auto& mesh : model.meshes)
            {
                appended_any_mesh |= append_transformed_mesh(
                    mesh,
                    Mat4(1.0F),
                    flattened_vertices,
                    flattened_indices,
                    flattened_layout,
                    has_layout);
            }
        }
        else
        {
            auto has_parent = std::vector<bool>(model.parts.size(), false);
            for (const auto& part : model.parts)
            {
                for (const auto child_index : part.children)
                {
                    if (child_index < has_parent.size())
                        has_parent[child_index] = true;
                }
            }

            auto queue = std::vector<MeshPartQueueEntry> {};
            queue.reserve(model.parts.size());
            for (size_t part_index = 0U; part_index < model.parts.size(); ++part_index)
            {
                if (has_parent[part_index])
                    continue;

                queue.push_back(
                    MeshPartQueueEntry {
                        .part_index = part_index,
                        .parent_transform = Mat4(1.0F),
                    });
            }

            if (queue.empty())
            {
                queue.push_back(
                    MeshPartQueueEntry {
                        .part_index = 0U,
                        .parent_transform = Mat4(1.0F),
                    });
            }

            auto visited_parts = std::vector<bool>(model.parts.size(), false);
            while (!queue.empty())
            {
                const auto current = queue.back();
                queue.pop_back();
                if (current.part_index >= model.parts.size() || visited_parts[current.part_index])
                    continue;

                visited_parts[current.part_index] = true;
                const auto& part = model.parts[current.part_index];
                const auto part_transform = current.parent_transform * part.transform;
                if (part.mesh_index < model.meshes.size())
                {
                    appended_any_mesh |= append_transformed_mesh(
                        model.meshes[part.mesh_index],
                        part_transform,
                        flattened_vertices,
                        flattened_indices,
                        flattened_layout,
                        has_layout);
                }

                for (const auto child_index : part.children)
                {
                    queue.push_back(
                        MeshPartQueueEntry {
                            .part_index = child_index,
                            .parent_transform = part_transform,
                        });
                }
            }
        }

        if (!appended_any_mesh || !has_layout || flattened_indices.empty())
            return false;

        out_mesh.vertices.layout = flattened_layout;
        out_mesh.vertices.vertices = std::move(flattened_vertices);
        out_mesh.indices = std::move(flattened_indices);
        return true;
    }

    RenderResourceManager::RenderResourceManager(
        AssetManager& asset_manager,
        IGraphicsBackend& backend)
        : _asset_manager(asset_manager)
        , _backend(backend)
    {
    }

    Uuid RenderResourceManager::upload_dynamic_mesh(const DynamicMesh& dynamic_mesh, const bool pin)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto key = make_dynamic_mesh_key(dynamic_mesh.data);
        if (!key.is_valid() || !dynamic_mesh.data)
            return {};

        if (_resources.contains(key))
        {
            auto keep_existing = false;
            if (const auto source_it = _dynamic_mesh_sources.find(key);
                source_it != _dynamic_mesh_sources.end())
            {
                if (const auto source_mesh = source_it->second.lock())
                    keep_existing = source_mesh.get() == dynamic_mesh.data.get();
            }

            if (!keep_existing)
            {
                destroy_backend_resource(key);
                _resources.erase(key);
                _backend_resources.erase(key);
                _pinned_resources.erase(key);
                _last_access.erase(key);
                _dynamic_mesh_sources.erase(key);
            }
            else
            {
                _last_access.insert_or_assign(key, now);
                if (pin)
                    _pinned_resources.insert(key);
                return get_backend_resource_uuid(key);
            }
        }

        if (_resources.contains(key))
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert(key);
            return get_backend_resource_uuid(key);
        }

        auto backend_resource_uuid = Uuid {};
        const auto create_result = _backend.upload(*dynamic_mesh.data, backend_resource_uuid);
        if (!create_result || !backend_resource_uuid.is_valid())
            return {};

        _resources.insert(key);
        _backend_resources.insert_or_assign(key, backend_resource_uuid);
        _dynamic_mesh_sources.insert_or_assign(key, dynamic_mesh.data);
        _last_access.insert_or_assign(key, now);
        _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, key);
        if (pin)
            _pinned_resources.insert(key);
        return backend_resource_uuid;
    }

    Uuid RenderResourceManager::upload_static_mesh(const StaticMesh& static_mesh, const bool pin)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto resolved_mesh_handle = resolve_asset_handle(static_mesh.handle, _asset_manager);
        const auto key = make_static_mesh_key(resolved_mesh_handle);
        if (!key.is_valid())
            return {};

        if (_resources.contains(key))
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert(key);
            return get_backend_resource_uuid(key);
        }

        const auto model = _asset_manager.load<Model>(resolved_mesh_handle);
        if (!model || model->meshes.empty())
            return {};

        auto render_mesh = Mesh {};
        if (!try_build_static_mesh_render_geometry(*model, render_mesh))
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: failed to build render geometry for model '{}'.",
                resolved_mesh_handle.get_name().c_str());
            return {};
        }

        auto backend_resource_uuid = Uuid {};
        const auto create_result = _backend.upload(render_mesh, backend_resource_uuid);
        if (!create_result || !backend_resource_uuid.is_valid())
            return {};

        _resources.insert(key);
        _backend_resources.insert_or_assign(key, backend_resource_uuid);
        _last_access.insert_or_assign(key, now);
        _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, key);
        if (pin)
            _pinned_resources.insert(key);
        return backend_resource_uuid;
    }

    Uuid RenderResourceManager::upload_material(
        const MaterialInstance& material_instance,
        const bool pin)
    {
        const auto resolved_material_handle =
            resolve_asset_handle(material_instance.material, _asset_manager);
        if (!resolved_material_handle.is_valid())
        {
            TBX_TRACE_WARNING("Graphics rendering: material handle is invalid.");
            return {};
        }

        const auto now = std::chrono::steady_clock::now();
        const auto program_key = make_material_program_key(resolved_material_handle);
        if (!program_key.is_valid())
            return {};

        auto has_program = false;
        if (_resources.contains(program_key))
        {
            _last_access.insert_or_assign(program_key, now);
            if (pin)
                _pinned_resources.insert(program_key);
            has_program = true;
        }
        else if (const auto material = get_material_asset(resolved_material_handle))
        {
            const auto shader_handles = std::vector<Handle> {
                material->program.compute,
                material->program.vertex,
                material->program.fragment,
                material->program.tesselation,
                material->program.geometry,
            };
            auto backend_resource_uuid = Uuid {};
            const auto create_result = _backend.upload(*material, backend_resource_uuid);
            if (create_result && backend_resource_uuid.is_valid())
            {
                _resources.insert(program_key);
                _backend_resources.insert_or_assign(program_key, backend_resource_uuid);
                _last_access.insert_or_assign(program_key, now);
                _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, program_key);
                store_shader_dependencies(program_key, shader_handles);
                if (pin)
                    _pinned_resources.insert(program_key);
                has_program = true;
            }
            else
            {
                TBX_TRACE_WARNING(
                    "Graphics rendering: failed to build material resource for '{}'.",
                    resolved_material_handle.get_id().value);
            }
        }
        else
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: failed to load material '{}'.",
                resolved_material_handle.get_id().value);
        }

        if (has_program)
            return get_backend_resource_uuid(program_key);

        return {};
    }

    std::shared_ptr<Material> RenderResourceManager::get_material_asset(const Handle& material_handle)
    {
        const auto resolved_material_handle = resolve_asset_handle(material_handle, _asset_manager);
        if (!resolved_material_handle.is_valid())
            return {};

        const auto program_key = make_material_program_key(resolved_material_handle);
        if (!program_key.is_valid())
            return {};

        if (const auto existing = _material_assets.find(program_key); existing != _material_assets.end())
            return existing->second;

        const auto material = _asset_manager.load<Material>(resolved_material_handle);
        if (!material)
            return {};

        _material_assets.insert_or_assign(program_key, material);
        return material;
    }

    Uuid RenderResourceManager::upload_texture(
        const Texture& texture,
        const Uuid& resource_uuid,
        const bool pin)
    {
        if (!resource_uuid.is_valid())
            return {};

        const auto key = resolve_resource_key(resource_uuid);
        if (!key.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (_resources.contains(key))
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert(key);
            return get_backend_resource_uuid(key);
        }

        auto backend_resource_uuid = Uuid {};
        const auto create_result = _backend.upload(texture, backend_resource_uuid);
        if (!create_result || !backend_resource_uuid.is_valid())
            return {};

        _resources.insert(key);
        _backend_resources.insert_or_assign(key, backend_resource_uuid);
        _last_access.insert_or_assign(key, now);
        _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, key);
        if (pin)
            _pinned_resources.insert(key);
        return backend_resource_uuid;
    }

    Uuid RenderResourceManager::upload_render_texture(
        const TextureSettings& texture_settings,
        const bool pin)
    {
        const auto key = make_render_texture_key(Uuid::generate());
        if (!key.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (_resources.contains(key))
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert(key);
            return get_backend_resource_uuid(key);
        }

        auto backend_resource_uuid = Uuid {};
        const auto create_result = _backend.upload(texture_settings, backend_resource_uuid);
        if (!create_result || !backend_resource_uuid.is_valid())
            return {};

        _resources.insert(key);
        _backend_resources.insert_or_assign(key, backend_resource_uuid);
        _last_access.insert_or_assign(key, now);
        _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, key);
        if (pin)
            _pinned_resources.insert(key);
        return backend_resource_uuid;
    }

    Uuid RenderResourceManager::upload_texture(const Handle& texture_handle, const bool pin)
    {
        const auto resolved_texture_handle = resolve_asset_handle(texture_handle, _asset_manager);
        const auto key = make_texture_key(resolved_texture_handle);
        if (!key.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (_resources.contains(key))
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert(key);
            return get_backend_resource_uuid(key);
        }

        const auto texture = _asset_manager.load<Texture>(resolved_texture_handle);
        if (!texture)
            return {};

        auto backend_resource_uuid = Uuid {};
        const auto create_result = _backend.upload(*texture, backend_resource_uuid);
        if (!create_result || !backend_resource_uuid.is_valid())
            return {};

        _resources.insert(key);
        _backend_resources.insert_or_assign(key, backend_resource_uuid);
        _last_access.insert_or_assign(key, now);
        _resource_keys_by_backend.insert_or_assign(backend_resource_uuid, key);
        if (pin)
            _pinned_resources.insert(key);
        return backend_resource_uuid;
    }

    void RenderResourceManager::unload(const Uuid& resource_uuid)
    {
        invalidate_resource(resolve_resource_key(resource_uuid));
    }

    void RenderResourceManager::on_asset_reloaded(const Handle& asset_handle)
    {
        if (!asset_handle.get_id().is_valid())
            return;

        invalidate_resource(make_static_mesh_key(asset_handle));
        invalidate_resource(make_texture_key(asset_handle));
        invalidate_material_program(make_material_program_key(asset_handle));

        const auto shader_iterator = _programs_by_shader.find(asset_handle.get_id());
        if (shader_iterator == _programs_by_shader.end())
            return;

        const auto dependent_programs = shader_iterator->second;
        for (const auto& program_key : dependent_programs)
            invalidate_material_program(program_key);
    }

    void RenderResourceManager::invalidate_material_program(const Uuid& program_key)
    {
        if (!program_key.is_valid())
            return;

        clear_shader_dependencies(program_key);
        _material_assets.erase(program_key);
        invalidate_resource(program_key);
    }

    void RenderResourceManager::invalidate_resource(const Uuid& resource_key)
    {
        if (!resource_key.is_valid())
            return;

        clear_shader_dependencies(resource_key);
        destroy_backend_resource(resource_key);
        _dynamic_mesh_sources.erase(resource_key);
        _material_assets.erase(resource_key);
        _resources.erase(resource_key);
        _pinned_resources.erase(resource_key);
        _last_access.erase(resource_key);
    }

    void RenderResourceManager::destroy_backend_resource(const Uuid& resource_key)
    {
        if (!resource_key.is_valid())
            return;

        const auto backend_resource_uuid = get_backend_resource_uuid(resource_key);
        if (!backend_resource_uuid.is_valid())
            return;

        const auto destroy_result = _backend.unload(backend_resource_uuid);
        if (!destroy_result)
        {
            TBX_TRACE_WARNING(
                "Graphics rendering: backend destroy failed for resource '{}': {}",
                backend_resource_uuid.value,
                destroy_result.get_report());
        }

        _resource_keys_by_backend.erase(backend_resource_uuid);
        _backend_resources.erase(resource_key);
    }

    void RenderResourceManager::clear_shader_dependencies(const Uuid& program_key)
    {
        const auto shader_list_iterator = _shaders_by_program.find(program_key);
        if (shader_list_iterator == _shaders_by_program.end())
            return;

        for (const auto& shader_id : shader_list_iterator->second)
        {
            const auto programs_iterator = _programs_by_shader.find(shader_id);
            if (programs_iterator == _programs_by_shader.end())
                continue;

            auto& programs = programs_iterator->second;
            std::erase(programs, program_key);
            if (programs.empty())
                _programs_by_shader.erase(programs_iterator);
        }

        _shaders_by_program.erase(shader_list_iterator);
    }

    void RenderResourceManager::store_shader_dependencies(
        const Uuid& program_key,
        const std::vector<Handle>& shader_handles)
    {
        clear_shader_dependencies(program_key);

        auto shader_ids = std::vector<Uuid> {};
        shader_ids.reserve(shader_handles.size());
        for (const auto& shader_handle : shader_handles)
        {
            if (!shader_handle.get_id().is_valid())
                continue;

            append_unique_uuid(shader_ids, shader_handle.get_id());
            auto& programs = _programs_by_shader[shader_handle.get_id()];
            append_unique_uuid(programs, program_key);
        }

        if (shader_ids.empty())
            return;

        _shaders_by_program.insert_or_assign(program_key, std::move(shader_ids));
    }

    void RenderResourceManager::pin(const Uuid& resource_uuid)
    {
        const auto resource_key = resolve_resource_key(resource_uuid);
        if (!_resources.contains(resource_key))
            return;

        _pinned_resources.insert(resource_key);
    }

    void RenderResourceManager::unpin(const Uuid& resource_uuid)
    {
        _pinned_resources.erase(resolve_resource_key(resource_uuid));
    }

    void RenderResourceManager::clear()
    {
        for (const auto& resource_key : _resources)
            destroy_backend_resource(resource_key);
        _resources.clear();
        _backend_resources.clear();
        _dynamic_mesh_sources.clear();
        _material_assets.clear();
        _pinned_resources.clear();
        _last_access.clear();
        _programs_by_shader.clear();
        _resource_keys_by_backend.clear();
        _shaders_by_program.clear();
    }

    void RenderResourceManager::clear_unused()
    {
        auto resources_to_remove = std::vector<Uuid> {};
        resources_to_remove.reserve(_resources.size());
        for (const auto& resource_key : _resources)
        {
            if (_pinned_resources.contains(resource_key))
                continue;

            const auto material_asset_it = _material_assets.find(resource_key);
            const auto dynamic_mesh_it = _dynamic_mesh_sources.find(resource_key);
            const auto has_live_material_asset =
                material_asset_it != _material_assets.end() && static_cast<bool>(material_asset_it->second);
            const auto has_live_dynamic_mesh =
                dynamic_mesh_it != _dynamic_mesh_sources.end() && !dynamic_mesh_it->second.expired();
            if (has_live_material_asset || has_live_dynamic_mesh)
                continue;

            resources_to_remove.push_back(resource_key);
        }

        for (const auto& resource_key : resources_to_remove)
            invalidate_resource(resource_key);

        std::erase_if(
            _material_assets,
            [this](const auto& entry)
            {
                return !_resources.contains(entry.first);
            });

        std::erase_if(
            _dynamic_mesh_sources,
            [this](const auto& entry)
            {
                const auto key = entry.first;
                return !_resources.contains(key) || entry.second.expired();
            });

        std::erase_if(
            _last_access,
            [this](const auto& entry)
            {
                const auto key = entry.first;
                return !_resources.contains(key);
            });

        std::erase_if(
            _shaders_by_program,
            [this](const auto& entry)
            {
                const auto key = entry.first;
                return !_resources.contains(key);
            });

        for (auto iterator = _programs_by_shader.begin(); iterator != _programs_by_shader.end();)
        {
            auto& programs = iterator->second;
            std::erase_if(
                programs,
                [this](const auto& program_key)
                {
                    return !_resources.contains(program_key);
                });

            if (programs.empty())
            {
                iterator = _programs_by_shader.erase(iterator);
                continue;
            }

            ++iterator;
        }
    }

    Uuid RenderResourceManager::get_backend_resource_uuid(const Uuid& resource_key) const
    {
        if (!resource_key.is_valid())
            return {};

        if (const auto it = _backend_resources.find(resource_key); it != _backend_resources.end())
            return it->second;

        return {};
    }

    Uuid RenderResourceManager::resolve_resource_key(const Uuid& resource_uuid) const
    {
        if (!resource_uuid.is_valid())
            return {};

        if (_resources.contains(resource_uuid))
            return resource_uuid;

        if (const auto it = _resource_keys_by_backend.find(resource_uuid);
            it != _resource_keys_by_backend.end())
        {
            return it->second;
        }

        return resource_uuid;
    }
}
