#include "opengl_resource_manager.h"
#include "opengl_mesh.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include <variant>

namespace opengl_rendering
{
    constexpr uint32 DynamicMeshSalt = 0xD1A60001U;
    constexpr uint32 StaticMeshSalt = 0x57A71C01U;
    constexpr uint32 TextureSalt = 0x7EAE0001U;
    constexpr uint32 MaterialProgramSalt = 0xAA110001U;

    struct MeshPartQueueEntry final
    {
        size_t part_index = 0U;
        tbx::Mat4 parent_transform = tbx::Mat4(1.0F);
    };

    tbx::Uuid make_non_zero_uuid_from_hash(const std::size_t hash_value)
    {
        auto hashed = static_cast<uint32>(hash_value);
        if (hashed == 0U)
            hashed = 1U;
        return tbx::Uuid(hashed);
    }

    tbx::Uuid make_typed_key(const tbx::Uuid& base_key, const uint32 salt)
    {
        if (!base_key.is_valid())
            return {};

        auto typed_key = tbx::Uuid(base_key);
        typed_key.combine(salt);
        if (!typed_key.is_valid())
            return tbx::Uuid(1U);
        return typed_key;
    }

    tbx::Uuid make_dynamic_mesh_key(const std::shared_ptr<tbx::Mesh>& mesh)
    {
        if (!mesh)
            return {};

        auto* mesh_address = mesh.get();
        const auto hash_value =
            std::hash<std::uintptr_t>()(reinterpret_cast<std::uintptr_t>(mesh_address));
        return make_typed_key(make_non_zero_uuid_from_hash(hash_value), DynamicMeshSalt);
    }

    tbx::Uuid make_static_mesh_key(const tbx::Handle& mesh_handle)
    {
        return make_typed_key(mesh_handle.id, StaticMeshSalt);
    }

    tbx::Uuid make_texture_key(const tbx::Handle& texture_handle)
    {
        return make_typed_key(texture_handle.id, TextureSalt);
    }

    tbx::Uuid make_material_program_key(const tbx::Handle& material_handle)
    {
        return make_typed_key(material_handle.id, MaterialProgramSalt);
    }

    static bool try_get_vec3_attribute_offsets(
        const tbx::VertexBufferLayout& layout,
        size_t& out_position_offset_floats,
        size_t& out_normal_offset_floats,
        bool& out_has_normal)
    {
        out_position_offset_floats = 0U;
        out_normal_offset_floats = 0U;
        out_has_normal = false;

        auto vec3_attribute_index = size_t {0U};
        for (const auto& attribute : layout.elements)
        {
            if (!std::holds_alternative<tbx::Vec3>(attribute.type))
                continue;

            const auto attribute_offset_floats =
                static_cast<size_t>(attribute.offset) / sizeof(float);
            if (vec3_attribute_index == 0U)
                out_position_offset_floats = attribute_offset_floats;
            else if (vec3_attribute_index == 1U)
            {
                out_normal_offset_floats = attribute_offset_floats;
                out_has_normal = true;
                return true;
            }

            vec3_attribute_index += 1U;
        }

        return vec3_attribute_index > 0U;
    }

    static bool append_transformed_mesh(
        const tbx::Mesh& mesh,
        const tbx::Mat4& transform,
        std::vector<float>& out_vertices,
        std::vector<uint32>& out_indices,
        tbx::VertexBufferLayout& out_layout,
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
        if (!try_get_vec3_attribute_offsets(
                source_layout,
                position_offset_floats,
                normal_offset_floats,
                has_normal))
            return false;

        if (!out_has_layout)
        {
            out_layout = source_layout;
            out_has_layout = true;
        }
        else if (out_layout.stride != source_layout.stride)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: skipping mesh append due to mismatched model vertex layouts.");
            return false;
        }

        const auto base_vertex_index =
            static_cast<uint32>(out_vertices.size() / stride_floats);
        out_vertices.reserve(out_vertices.size() + source_vertices.size());

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
                * tbx::Vec4(
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
                transform
                * tbx::Vec4(
                    source_vertices[source_base_index + normal_offset_floats],
                    source_vertices[source_base_index + normal_offset_floats + 1U],
                    source_vertices[source_base_index + normal_offset_floats + 2U],
                    0.0F);
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
        }

        out_indices.reserve(out_indices.size() + mesh.indices.size());
        for (const auto index : mesh.indices)
            out_indices.push_back(base_vertex_index + index);

        return true;
    }

    static bool try_build_static_mesh_render_geometry(
        const tbx::Model& model,
        tbx::Mesh& out_mesh)
    {
        if (model.meshes.empty())
            return false;

        auto flattened_vertices = std::vector<float> {};
        auto flattened_indices = std::vector<uint32> {};
        auto flattened_layout = tbx::VertexBufferLayout {};
        auto has_layout = false;
        auto appended_any_mesh = false;

        if (model.parts.empty())
        {
            for (const auto& mesh : model.meshes)
            {
                appended_any_mesh |= append_transformed_mesh(
                    mesh,
                    tbx::Mat4(1.0F),
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
                        .parent_transform = tbx::Mat4(1.0F),
                    });
            }

            if (queue.empty())
            {
                queue.push_back(
                    MeshPartQueueEntry {
                        .part_index = 0U,
                        .parent_transform = tbx::Mat4(1.0F),
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

    OpenGlResourceManager::OpenGlResourceManager(tbx::AssetManager& asset_manager)
        : _asset_manager(asset_manager)
    {
    }

    tbx::Uuid OpenGlResourceManager::add_dynamic_mesh(
        const tbx::DynamicMesh& dynamic_mesh,
        const bool pin)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto key = make_dynamic_mesh_key(dynamic_mesh.data);
        if (!key.is_valid())
            return {};

        if (!dynamic_mesh.data)
            return {};

        if (const auto existing = _resources.find(key); existing != _resources.end())
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
                _resources.erase(key);
                _pinned_resources.erase(key);
                _last_access.erase(key);
                _dynamic_mesh_sources.erase(key);
            }
            else
            {
                _last_access.insert_or_assign(key, now);
                if (pin)
                    _pinned_resources.insert_or_assign(key, existing->second);
                return key;
            }
        }

        if (const auto existing = _resources.find(key); existing != _resources.end())
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert_or_assign(key, existing->second);
            return key;
        }

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlMesh>(*dynamic_mesh.data));
        _resources.insert_or_assign(key, resource);
        _dynamic_mesh_sources.insert_or_assign(key, dynamic_mesh.data);
        _last_access.insert_or_assign(key, now);
        if (pin)
            _pinned_resources.insert_or_assign(key, resource);
        return key;
    }

    tbx::Uuid OpenGlResourceManager::add_static_mesh(
        const tbx::StaticMesh& static_mesh,
        const bool pin)
    {
        const auto now = std::chrono::steady_clock::now();
        const auto key = make_static_mesh_key(static_mesh.handle);
        if (!key.is_valid())
            return {};

        if (const auto existing = _resources.find(key); existing != _resources.end())
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert_or_assign(key, existing->second);
            return key;
        }

        const auto model = _asset_manager.load<tbx::Model>(static_mesh.handle);
        if (!model || model->meshes.empty())
            return {};

        auto render_mesh = tbx::Mesh {};
        if (!try_build_static_mesh_render_geometry(*model, render_mesh))
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to build render geometry for model '{}'.",
                static_mesh.handle.name.c_str());
            return {};
        }

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlMesh>(render_mesh));
        _resources.insert_or_assign(key, resource);
        _last_access.insert_or_assign(key, now);
        if (pin)
            _pinned_resources.insert_or_assign(key, resource);
        return key;
    }

    tbx::Uuid OpenGlResourceManager::add_material(
        const tbx::MaterialInstance& material_instance,
        const bool pin)
    {
        if (!material_instance.material.is_valid())
        {
            TBX_TRACE_WARNING("OpenGL rendering: material handle is invalid.");
            return {};
        }

        const auto now = std::chrono::steady_clock::now();
        const auto program_key = make_material_program_key(material_instance.material);
        if (!program_key.is_valid())
            return {};

        auto has_program = false;
        if (const auto existing = _resources.find(program_key); existing != _resources.end())
        {
            _last_access.insert_or_assign(program_key, now);
            if (pin)
                _pinned_resources.insert_or_assign(program_key, existing->second);
            has_program = true;
        }
        else if (const auto material = get_material_asset(material_instance.material))
        {
            auto shader_resources = std::vector<std::shared_ptr<OpenGlShader>>();
            auto try_append_shader = [&](const tbx::Handle& shader_handle)
            {
                if (!shader_handle.is_valid())
                    return true;

                const auto shader = _asset_manager.load<tbx::Shader>(shader_handle);
                if (!shader)
                {
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: failed to load shader '{}' for material '{}'.",
                        shader_handle.id.value,
                        material_instance.material.id.value);
                    return false;
                }

                for (const auto& source : shader->sources)
                {
                    auto shader_resource = std::make_shared<OpenGlShader>(source);
                    if (!shader_resource->compile())
                    {
                        TBX_TRACE_WARNING(
                            "OpenGL rendering: failed to compile shader stage (type {}) for "
                            "material '{}'.",
                            static_cast<int>(source.type),
                            material_instance.material.id.value);
                        return false;
                    }
                    shader_resources.emplace_back(std::move(shader_resource));
                }
                if (shader->sources.empty())
                {
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: shader '{}' has no stages for material '{}'.",
                        shader_handle.id.value,
                        material_instance.material.id.value);
                    return false;
                }

                return true;
            };

            const auto has_compute = material->program.compute.is_valid();
            const auto appended_compute = try_append_shader(material->program.compute);
            const auto appended_vertex =
                has_compute ? true : try_append_shader(material->program.vertex);
            const auto appended_fragment =
                has_compute ? true : try_append_shader(material->program.fragment);
            const auto appended_tesselation =
                has_compute ? true : try_append_shader(material->program.tesselation);
            const auto appended_geometry =
                has_compute ? true : try_append_shader(material->program.geometry);

            if ((has_compute && appended_compute)
                || (!has_compute && appended_vertex && appended_fragment
                    && appended_tesselation && appended_geometry))
            {
                const auto shader_program = std::make_shared<OpenGlShaderProgram>(shader_resources);
                if (shader_program->get_program_id() != 0)
                {
                    const auto resource = std::shared_ptr<IOpenGlResource>(shader_program);
                    _resources.insert_or_assign(program_key, resource);
                    _last_access.insert_or_assign(program_key, now);
                    if (pin)
                        _pinned_resources.insert_or_assign(program_key, resource);
                    has_program = true;
                }
            }

            if (!has_program)
            {
                TBX_TRACE_WARNING(
                    "OpenGL rendering: failed to build shader program for material '{}'.",
                    material_instance.material.id.value);
            }
        }
        else
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to load material '{}'.",
                material_instance.material.id.value);
        }

        if (has_program)
            return program_key;

        return {};
    }

    std::shared_ptr<tbx::Material> OpenGlResourceManager::get_material_asset(
        const tbx::Handle& material_handle)
    {
        if (!material_handle.is_valid())
            return {};

        const auto program_key = make_material_program_key(material_handle);
        if (!program_key.is_valid())
            return {};

        if (const auto existing = _material_assets.find(program_key); existing != _material_assets.end())
            return existing->second;

        const auto material = _asset_manager.load<tbx::Material>(material_handle);
        if (!material)
            return {};

        _material_assets.insert_or_assign(program_key, material);
        return material;
    }

    tbx::Uuid OpenGlResourceManager::add_material(
        const std::shared_ptr<OpenGlShaderProgram>& shader_program,
        const tbx::Uuid& resource_uuid,
        const bool pin)
    {
        if (!resource_uuid.is_valid())
            return {};
        if (!shader_program || shader_program->get_program_id() == 0)
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (const auto existing = _resources.find(resource_uuid);
            existing != _resources.end())
        {
            _last_access.insert_or_assign(resource_uuid, now);
            if (pin)
                _pinned_resources.insert_or_assign(resource_uuid, existing->second);
            return resource_uuid;
        }

        const auto resource = std::shared_ptr<IOpenGlResource>(shader_program);
        _resources.insert_or_assign(resource_uuid, resource);
        _last_access.insert_or_assign(resource_uuid, now);
        if (pin)
            _pinned_resources.insert_or_assign(resource_uuid, resource);
        return resource_uuid;
    }

    tbx::Uuid OpenGlResourceManager::add_texture(
        const tbx::Texture& texture,
        const tbx::Uuid& resource_uuid,
        const bool pin)
    {
        if (!resource_uuid.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (const auto existing = _resources.find(resource_uuid); existing != _resources.end())
        {
            _last_access.insert_or_assign(resource_uuid, now);
            if (pin)
                _pinned_resources.insert_or_assign(resource_uuid, existing->second);
            return resource_uuid;
        }

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlTexture>(texture));
        _resources.insert_or_assign(resource_uuid, resource);
        _last_access.insert_or_assign(resource_uuid, now);
        if (pin)
            _pinned_resources.insert_or_assign(resource_uuid, resource);
        return resource_uuid;
    }

    tbx::Uuid OpenGlResourceManager::add_texture(const tbx::Handle& texture_handle, const bool pin)
    {
        const auto key = make_texture_key(texture_handle);
        if (!key.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        if (const auto existing = _resources.find(key); existing != _resources.end())
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert_or_assign(key, existing->second);
            return key;
        }

        const auto texture = _asset_manager.load<tbx::Texture>(texture_handle);
        if (!texture)
            return {};

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlTexture>(*texture));
        _resources.insert_or_assign(key, resource);
        _last_access.insert_or_assign(key, now);
        if (pin)
            _pinned_resources.insert_or_assign(key, resource);
        return key;
    }

    bool OpenGlResourceManager::try_get_raw(
        const tbx::Uuid& resource_uuid,
        std::shared_ptr<IOpenGlResource>& out_resource) const
    {
        const auto it = _resources.find(resource_uuid);
        if (it == _resources.end())
            return false;

        out_resource = it->second;
        return static_cast<bool>(out_resource);
    }

    void OpenGlResourceManager::pin(const tbx::Uuid& resource_uuid)
    {
        const auto it = _resources.find(resource_uuid);
        if (it == _resources.end())
            return;

        _pinned_resources.insert_or_assign(resource_uuid, it->second);
    }

    void OpenGlResourceManager::unpin(const tbx::Uuid& resource_uuid)
    {
        _pinned_resources.erase(resource_uuid);
    }

    void OpenGlResourceManager::clear()
    {
        _resources.clear();
        _dynamic_mesh_sources.clear();
        _material_assets.clear();
        _pinned_resources.clear();
        _last_access.clear();
    }

    void OpenGlResourceManager::clear_unused()
    {
        std::erase_if(
            _resources,
            [this](const auto& entry)
            {
                const auto key = entry.first;
                const auto& resource = entry.second;
                if (_pinned_resources.contains(key))
                    return false;
                return !resource || resource.use_count() <= 1;
            });

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
    }
}
