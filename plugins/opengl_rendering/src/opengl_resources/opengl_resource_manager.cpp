#include "opengl_resource_manager.h"
#include "opengl_mesh.h"
#include "opengl_shader.h"
#include "opengl_texture.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/renderer.h"

namespace tbx::plugins
{
    OpenGlResourceManager::OpenGlResourceManager(AssetManager& asset_manager)
        : _asset_manager(&asset_manager)
    {
        TBX_ASSERT(
            _asset_manager != nullptr,
            "OpenGL resource manager requires a valid asset manager reference.");
    }

    bool OpenGlResourceManager::try_load(
        const Entity& entity,
        std::vector<std::shared_ptr<IOpenGlResource>>& out_resources)
    {
        const auto resource_id = entity.get_id();
        auto iterator = _resources_by_entity.find(resource_id);
        if (iterator != _resources_by_entity.end())
        {
            iterator->second.last_use = Clock::now();
            out_resources = iterator->second.resources;
            return true;
        }

        auto resources = std::vector<std::shared_ptr<IOpenGlResource>> {};
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
        std::vector<std::shared_ptr<IOpenGlResource>>& out_resources)
    {
        if (!entity.has_component<Renderer>())
            return false;

        const auto& renderer = entity.get_component<Renderer>();

        if (entity.has_component<ProceduralMesh>())
        {
            const auto& procedural_mesh = entity.get_component<ProceduralMesh>();
            if (!procedural_mesh.mesh)
                return false;

            out_resources.push_back(std::make_shared<OpenGlMesh>(*procedural_mesh.mesh));
        }
        else if (entity.has_component<StaticMesh>())
        {
            const auto& static_mesh = entity.get_component<StaticMesh>();
            auto model = _asset_manager->load<Model>(static_mesh.model);
            if (!model || model->meshes.empty())
                return false;

            out_resources.push_back(std::make_shared<OpenGlMesh>(model->meshes[0]));
        }
        else
            return false;

        if (!renderer.material.is_valid())
            return !out_resources.empty();

        auto material = _asset_manager->load<Material>(renderer.material);
        if (!material)
            return !out_resources.empty();

        if (material->shader.is_valid())
        {
            auto shader_stages = std::vector<std::shared_ptr<OpenGlShader>> {};

            const auto append_shader_stages = [this, &shader_stages](const Handle& shader_handle)
            {
                if (!shader_handle.is_valid())
                    return;

                auto shader_asset = _asset_manager->load<Shader>(shader_handle);
                if (!shader_asset)
                    return;

                for (const auto& source : shader_asset->sources)
                    shader_stages.push_back(std::make_shared<OpenGlShader>(source));
            };

            append_shader_stages(material->shader.vertex);
            append_shader_stages(material->shader.fragment);
            append_shader_stages(material->shader.compute);

            if (!shader_stages.empty())
                out_resources.push_back(std::make_shared<OpenGlShaderProgram>(shader_stages));
        }

        for (const auto& [_, texture_handle] : material->textures)
        {
            if (!texture_handle.is_valid())
                continue;

            auto texture = _asset_manager->load<Texture>(texture_handle);
            if (texture)
                out_resources.push_back(std::make_shared<OpenGlTexture>(*texture));
        }

        return !out_resources.empty();
    }
}
