#include "opengl_resource_manager.h"
#include "opengl_mesh.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/mesh.h"
#include "tbx/graphics/model.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace opengl_rendering
{
    constexpr tbx::uint32 DynamicMeshSalt = 0xD1A60001U;
    constexpr tbx::uint32 StaticMeshSalt = 0x57A71C01U;
    constexpr tbx::uint32 TextureSalt = 0x7EAE0001U;
    constexpr tbx::uint32 MaterialProgramSalt = 0xAA110001U;

    tbx::Uuid make_non_zero_uuid_from_hash(const std::size_t hash_value)
    {
        auto hashed = static_cast<tbx::uint32>(hash_value);
        if (hashed == 0U)
            hashed = 1U;
        return tbx::Uuid(hashed);
    }

    tbx::Uuid make_typed_key(const tbx::Uuid& base_key, const tbx::uint32 salt)
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

        if (const auto existing = _resources.find(key); existing != _resources.end())
        {
            _last_access.insert_or_assign(key, now);
            if (pin)
                _pinned_resources.insert_or_assign(key, existing->second);
            return key;
        }

        if (!dynamic_mesh.data)
            return {};

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlMesh>(*dynamic_mesh.data));
        _resources.insert_or_assign(key, resource);
        _last_access.insert_or_assign(key, now);
        if (pin)
            _pinned_resources.insert_or_assign(key, resource);
        return key;
    }

    tbx::Uuid
        OpenGlResourceManager::add_static_mesh(const tbx::StaticMesh& static_mesh, const bool pin)
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

        const auto resource =
            std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlMesh>(model->meshes.front()));
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
        if (!material_instance.handle.is_valid())
            return {};

        const auto now = std::chrono::steady_clock::now();
        const auto program_key = make_material_program_key(material_instance.handle);
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
        else if (
            const auto material = _asset_manager.load<tbx::Material>(material_instance.handle))
        {
            auto shader_resources = std::vector<std::shared_ptr<OpenGlShader>>();
            auto try_append_shader = [&](const tbx::Handle& shader_handle)
            {
                if (!shader_handle.is_valid())
                    return true;

                const auto shader = _asset_manager.load<tbx::Shader>(shader_handle);
                if (!shader)
                    return false;

                for (const auto& source : shader->sources)
                    shader_resources.emplace_back(std::make_shared<OpenGlShader>(source));
                return !shader->sources.empty();
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

            if (
                (has_compute && appended_compute)
                || (!has_compute && appended_vertex && appended_fragment))
            {
                (void)appended_tesselation;
                (void)appended_geometry;
                const auto resource = std::shared_ptr<IOpenGlResource>(
                    std::make_shared<OpenGlShaderProgram>(shader_resources));
                _resources.insert_or_assign(program_key, resource);
                _last_access.insert_or_assign(program_key, now);
                if (pin)
                    _pinned_resources.insert_or_assign(program_key, resource);
                has_program = true;
            }
        }

        for (const auto& texture_binding : material_instance.textures.values)
        {
            const auto texture_key = make_texture_key(texture_binding.texture.handle);
            if (!texture_key.is_valid())
                continue;

            if (const auto texture_it = _resources.find(texture_key);
                texture_it != _resources.end())
            {
                _last_access.insert_or_assign(texture_key, now);
                if (pin)
                    _pinned_resources.insert_or_assign(texture_key, texture_it->second);
                continue;
            }

            if (const auto texture =
                    _asset_manager.load<tbx::Texture>(texture_binding.texture.handle))
            {
                const auto resource =
                    std::shared_ptr<IOpenGlResource>(std::make_shared<OpenGlTexture>(*texture));
                _resources.insert_or_assign(texture_key, resource);
                _last_access.insert_or_assign(texture_key, now);
                if (pin)
                    _pinned_resources.insert_or_assign(texture_key, resource);
            }
        }

        if (has_program)
            return program_key;
        return {};
    }

    bool OpenGlResourceManager::try_get(
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
            _last_access,
            [this](const auto& entry)
            {
                const auto key = entry.first;
                return !_resources.contains(key);
            });
    }
}
