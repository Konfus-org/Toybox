#include "opengl_resource_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"

namespace tbx::plugins
{
    static std::string normalize_uniform_name(std::string_view name)
    {
        if (name.size() >= 2U && name[0] == 'u' && name[1] == '_')
            return std::string(name);

        std::string normalized = "u_";
        normalized.append(name.begin(), name.end());
        return normalized;
    }

    OpenGlResourceManager::OpenGlResourceManager(AssetManager& asset_manager)
        : _asset_manager(&asset_manager)
    {
        TBX_ASSERT(
            _asset_manager != nullptr,
            "OpenGL resource manager requires a valid asset manager reference.");
    }

    bool OpenGlResourceManager::try_load(const Entity& entity, OpenGlDrawResources& out_resources)
    {
        const auto resource_id = entity.get_id();
        auto iterator = _resources_by_entity.find(resource_id);
        if (iterator != _resources_by_entity.end())
        {
            iterator->second.last_use = Clock::now();
            out_resources = iterator->second.resources;
            return true;
        }

        auto resources = OpenGlDrawResources {};
        if (!try_create_resources(entity, resources))
            return false;

        _resources_by_entity[resource_id] = CachedEntityResources {
            .resources = resources,
            .last_use = Clock::now(),
        };
        out_resources = std::move(resources);
        return true;
    }

    void OpenGlResourceManager::clear()
    {
        _resources_by_entity.clear();
    }

    void OpenGlResourceManager::unload_unreferenced()
    {
        const auto unload_before = Clock::now() - UNUSED_TTL;

        for (auto iterator = _resources_by_entity.begin(); iterator != _resources_by_entity.end();)
        {
            if (iterator->second.last_use >= unload_before)
            {
                ++iterator;
                continue;
            }

            iterator = _resources_by_entity.erase(iterator);
        }
    }

    bool OpenGlResourceManager::try_create_resources(
        const Entity& entity,
        OpenGlDrawResources& out_resources)
    {
        const auto& renderer = entity.get_component<Renderer>();
        if (entity.has_component<StaticMesh>())
            return try_create_static_mesh_resources(entity, renderer, out_resources);
        if (entity.has_component<DynamicMesh>())
            return try_create_dynamic_mesh_resources(entity, renderer, out_resources);

        return false;
    }

    bool OpenGlResourceManager::try_create_static_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& static_mesh = entity.get_component<StaticMesh>();
        auto model = _asset_manager->load<Model>(static_mesh.model);
        if (!model || model->meshes.empty())
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(model->meshes[0]);
        return try_append_material_resources(renderer, out_resources);
    }

    bool OpenGlResourceManager::try_create_dynamic_mesh_resources(
        const Entity& entity,
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        const auto& dynamic_mesh = entity.get_component<DynamicMesh>();
        if (!dynamic_mesh.mesh)
            return false;

        out_resources.mesh = std::make_shared<OpenGlMesh>(*dynamic_mesh.mesh);
        return try_append_material_resources(renderer, out_resources);
    }

    bool OpenGlResourceManager::try_append_material_resources(
        const Renderer& renderer,
        OpenGlDrawResources& out_resources)
    {
        if (!renderer.material.is_valid())
            return out_resources.mesh != nullptr;

        auto material = _asset_manager->load<Material>(renderer.material);
        if (!material)
            return out_resources.mesh != nullptr;

        if (material->shader.is_valid())
        {
            auto shader_stages = std::vector<std::shared_ptr<OpenGlShader>> {};

            const auto append_shader_stages =
                [this, &shader_stages](const Handle& shader_handle, ShaderType expected_type)
            {
                if (!shader_handle.is_valid())
                    return;

                auto shader_asset = _asset_manager->load<Shader>(shader_handle);
                if (!shader_asset)
                    return;

                const ShaderSource* stage_source = nullptr;
                for (const auto& source : shader_asset->sources)
                {
                    if (source.type != expected_type)
                    {
                        TBX_ASSERT(
                            false,
                            "OpenGL rendering: shader source type does not match expected stage "
                            "type.");
                        continue;
                    }

                    stage_source = &source;
                    break;
                }

                if (!stage_source)
                    return;

                shader_stages.push_back(std::make_shared<OpenGlShader>(*stage_source));
            };

            append_shader_stages(material->shader.vertex, ShaderType::VERTEX);
            append_shader_stages(material->shader.fragment, ShaderType::FRAGMENT);
            append_shader_stages(material->shader.compute, ShaderType::COMPUTE);

            if (!shader_stages.empty())
                out_resources.shader_program = std::make_shared<OpenGlShaderProgram>(shader_stages);
        }

        for (const auto& [texture_name, texture_handle] : material->textures)
        {
            if (!texture_handle.is_valid())
                continue;

            auto texture_asset = _asset_manager->load<Texture>(texture_handle);
            if (!texture_asset)
                continue;

            auto texture = std::make_shared<OpenGlTexture>(*texture_asset);
            auto texture_slot = static_cast<int>(out_resources.textures.size());
            texture->set_slot(static_cast<uint32>(texture_slot));
            out_resources.textures.push_back(
                OpenGlTextureBinding {
                    .uniform_name = normalize_uniform_name(texture_name),
                    .slot = texture_slot,
                    .texture = texture,
                });
        }

        for (const auto& [parameter_name, parameter_value] : material->parameters)
        {
            out_resources.shader_parameters.push_back(
                ShaderUniform {
                    .name = normalize_uniform_name(parameter_name),
                    .data = parameter_value,
                });
        }

        if (!out_resources.shader_program)
            return out_resources.mesh != nullptr;

        auto shader_program_scope = GlResourceScope(*out_resources.shader_program);
        for (const auto& texture_binding : out_resources.textures)
        {
            out_resources.shader_program->try_upload(
                ShaderUniform {
                    .name = texture_binding.uniform_name,
                    .data = texture_binding.slot,
                });
        }

        return out_resources.mesh != nullptr;
    }
}
