#include "opengl_render_cache.h"
#include "tbx/assets/builtin_assets.h"
#include "tbx/debugging/macros.h"
#include <string>
#include <vector>

namespace tbx::plugins
{
    static std::string to_uniform_name(const std::string& material_name)
    {
        if (material_name.size() >= 2U && material_name[0] == 'u' && material_name[1] == '_')
        {
            return material_name;
        }
        return "u_" + material_name;
    }

    static void ensure_default_shader_textures(OpenGlMaterial& material)
    {
        if (material.shader_programs.size() != 1U
            || material.shader_programs.front() != default_shader.id)
        {
            return;
        }

        std::string diffuse_name = to_uniform_name("diffuse");
        std::string normal_name = to_uniform_name("normal");

        OpenGlMaterialTexture diffuse = {.name = diffuse_name};
        OpenGlMaterialTexture normal = {.name = normal_name};

        for (const auto& texture : material.textures)
        {
            if (texture.name == diffuse_name)
            {
                diffuse = texture;
            }
            else if (texture.name == normal_name)
            {
                normal = texture;
            }
        }

        std::vector<OpenGlMaterialTexture> reordered = {};
        reordered.reserve(material.textures.size() + 2U);
        reordered.push_back(std::move(diffuse));
        reordered.push_back(std::move(normal));

        for (const auto& texture : material.textures)
        {
            if (texture.name == diffuse_name || texture.name == normal_name)
            {
                continue;
            }
            reordered.push_back(texture);
        }

        material.textures = std::move(reordered);
    }

    void OpenGlRenderCache::clear()
    {
        _meshes.clear();
        _shaders.clear();
        _shader_programs.clear();
        _textures.clear();
        _models.clear();
        _materials.clear();
        _fallback_material = {};
        _has_fallback_material = false;
        _default_texture.reset();
    }

    OpenGlModel* OpenGlRenderCache::get_cached_model(
        AssetManager& asset_manager,
        const Handle& handle,
        const GlIdProvider& id_provider)
    {
        if (!handle.is_valid())
        {
            return nullptr;
        }

        Uuid model_id = asset_manager.resolve_asset_id(handle);
        auto model_it = _models.find(model_id);
        if (model_it != _models.end())
        {
            return &model_it->second;
        }

        auto model_asset = asset_manager.load<Model>(handle);
        if (!model_asset)
        {
            return nullptr;
        }

        OpenGlModel gl_model = {};
        gl_model.model_id = model_id;
        gl_model.meshes.reserve(model_asset->meshes.size());
        gl_model.parts.reserve(model_asset->parts.size());

        for (size_t mesh_index = 0U; mesh_index < model_asset->meshes.size(); ++mesh_index)
        {
            const Mesh& mesh = model_asset->meshes[mesh_index];
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                gl_model.meshes.push_back({});
                continue;
            }

            Uuid mesh_key = id_provider.provide(model_id, static_cast<uint32>(mesh_index));
            get_mesh(mesh, mesh_key);
            gl_model.meshes.push_back(mesh_key);
        }

        for (const auto& part : model_asset->parts)
        {
            OpenGlModelPart gl_part = {};
            gl_part.transform = part.transform;
            gl_part.children = part.children;

            if (part.mesh_index < gl_model.meshes.size())
            {
                gl_part.mesh_id = gl_model.meshes[part.mesh_index];
            }

            if (part.material_index < model_asset->materials.size())
            {
                Uuid material_key =
                    id_provider.provide(model_id, static_cast<uint32>(part.material_index));
                gl_part.material_id = material_key;

                if (_materials.find(material_key) == _materials.end())
                {
                    OpenGlMaterial gl_material = {};
                    const Material& material = model_asset->materials[part.material_index];
                    if (try_build_gl_material(asset_manager, material, gl_material, id_provider))
                    {
                        _materials.emplace(material_key, std::move(gl_material));
                    }
                    else
                    {
                        gl_part.material_id = {};
                    }
                }
            }

            gl_model.parts.push_back(std::move(gl_part));
        }

        auto [inserted, was_inserted] = _models.emplace(model_id, std::move(gl_model));
        static_cast<void>(was_inserted);
        return &inserted->second;
    }

    OpenGlMaterial* OpenGlRenderCache::get_cached_material(
        AssetManager& asset_manager,
        const Handle& handle,
        const std::shared_ptr<Material>& fallback_material,
        const GlIdProvider& id_provider)
    {
        Handle resolved = handle.is_valid() ? handle : default_material;
        Uuid material_id = asset_manager.resolve_asset_id(resolved);
        auto material_it = _materials.find(material_id);
        if (material_it != _materials.end())
        {
            return &material_it->second;
        }

        auto material_asset = asset_manager.load<Material>(resolved);
        if (!material_asset)
        {
            return get_cached_fallback_material(asset_manager, fallback_material, id_provider);
        }

        AssetUsage usage = asset_manager.get_usage<Material>(resolved);
        if (usage.stream_state != AssetStreamState::LOADED)
        {
            return resolved.id == default_material.id
                       ? get_cached_fallback_material(asset_manager, fallback_material, id_provider)
                       : get_cached_material(
                           asset_manager,
                           default_material,
                           fallback_material,
                           id_provider);
        }

        OpenGlMaterial gl_material = {};
        if (!try_build_gl_material(asset_manager, *material_asset, gl_material, id_provider))
        {
            return get_cached_fallback_material(asset_manager, fallback_material, id_provider);
        }

        auto [inserted, was_inserted] = _materials.emplace(material_id, std::move(gl_material));
        static_cast<void>(was_inserted);
        return &inserted->second;
    }

    OpenGlMaterial* OpenGlRenderCache::get_cached_fallback_material(
        AssetManager& asset_manager,
        const std::shared_ptr<Material>& fallback_material,
        const GlIdProvider& id_provider)
    {
        if (_has_fallback_material)
        {
            return &_fallback_material;
        }

        Material fallback = Material();
        const Material& source = fallback_material ? *fallback_material : fallback;
        OpenGlMaterial gl_material = {};
        if (try_build_gl_material(asset_manager, source, gl_material, id_provider))
        {
            _fallback_material = std::move(gl_material);
        }
        else
        {
            _fallback_material = {};
        }

        _has_fallback_material = true;
        return &_fallback_material;
    }

    bool OpenGlRenderCache::try_build_gl_material(
        AssetManager& asset_manager,
        const Material& source_material,
        OpenGlMaterial& out_material,
        const GlIdProvider& id_provider)
    {
        OpenGlMaterial gl_material = {};
        gl_material.parameters.reserve(source_material.parameters.size());
        for (const auto& parameter : source_material.parameters)
        {
            ShaderUniform mapped = parameter;
            mapped.name = to_uniform_name(parameter.name);
            gl_material.parameters.push_back(std::move(mapped));
        }
        gl_material.textures.reserve(source_material.textures.size());

        std::vector<Handle> shader_handles = source_material.shaders;
        if (shader_handles.empty())
        {
            shader_handles.push_back(default_shader);
        }

        for (auto shader_handle : shader_handles)
        {
            auto resolved_handle = shader_handle;
            if (!ensure_shader_program(asset_manager, resolved_handle, id_provider))
            {
                continue;
            }

            gl_material.shader_programs.push_back(asset_manager.resolve_asset_id(resolved_handle));
        }

        for (const auto& texture_binding : source_material.textures)
        {
            OpenGlMaterialTexture entry = {};
            entry.name = to_uniform_name(texture_binding.name);

            if (!texture_binding.handle.is_valid())
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            auto texture_asset = asset_manager.load<Texture>(texture_binding.handle);
            if (!texture_asset)
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            AssetUsage texture_usage = asset_manager.get_usage<Texture>(texture_binding.handle);
            if (texture_usage.stream_state != AssetStreamState::LOADED)
            {
                gl_material.textures.push_back(std::move(entry));
                continue;
            }

            Uuid texture_id = asset_manager.resolve_asset_id(texture_binding.handle);
            get_texture(texture_id, *texture_asset);
            entry.texture_id = texture_id;
            gl_material.textures.push_back(std::move(entry));
        }

        ensure_default_shader_textures(gl_material);

        if (gl_material.shader_programs.empty())
        {
            return false;
        }

        out_material = std::move(gl_material);
        return true;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderCache::ensure_shader_program(
        AssetManager& asset_manager,
        Handle& shader_handle,
        const GlIdProvider& id_provider)
    {
        if (!shader_handle.is_valid())
        {
            shader_handle = default_shader;
        }

        Uuid shader_id = asset_manager.resolve_asset_id(shader_handle);
        auto program_it = _shader_programs.find(shader_id);
        if (program_it != _shader_programs.end())
        {
            return program_it->second;
        }

        auto shader_asset = asset_manager.load<Shader>(shader_handle);
        if (!shader_asset)
        {
            return {};
        }

        AssetUsage shader_usage = asset_manager.get_usage<Shader>(shader_handle);
        if (shader_usage.stream_state == AssetStreamState::LOADED)
        {
            return get_shader_program(
                asset_manager.resolve_asset_id(shader_handle),
                *shader_asset,
                id_provider);
        }

        if (shader_handle.id == default_shader.id)
        {
            return {};
        }

        shader_handle = default_shader;
        Uuid fallback_id = asset_manager.resolve_asset_id(shader_handle);
        auto fallback_it = _shader_programs.find(fallback_id);
        if (fallback_it != _shader_programs.end())
        {
            return fallback_it->second;
        }

        shader_asset = asset_manager.load<Shader>(shader_handle);
        if (!shader_asset)
        {
            return {};
        }

        return get_shader_program(
            asset_manager.resolve_asset_id(shader_handle),
            *shader_asset,
            id_provider);
    }

    std::shared_ptr<OpenGlMesh> OpenGlRenderCache::get_cached_mesh(const Uuid& mesh_key) const
    {
        auto mesh_it = _meshes.find(mesh_key);
        if (mesh_it == _meshes.end())
        {
            return {};
        }

        return mesh_it->second;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderCache::get_cached_shader_program(
        const Uuid& shader_id) const
    {
        auto program_it = _shader_programs.find(shader_id);
        if (program_it == _shader_programs.end())
        {
            return {};
        }

        return program_it->second;
    }

    const OpenGlMaterial* OpenGlRenderCache::get_cached_material(const Uuid& material_id) const
    {
        auto material_it = _materials.find(material_id);
        if (material_it == _materials.end())
        {
            return nullptr;
        }

        return &material_it->second;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderCache::get_cached_texture(
        const Uuid& texture_id) const
    {
        auto texture_it = _textures.find(texture_id);
        if (texture_it == _textures.end())
        {
            return {};
        }

        return texture_it->second;
    }

    std::shared_ptr<OpenGlMesh> OpenGlRenderCache::get_mesh(
        const Mesh& mesh,
        const Uuid& mesh_key)
    {
        if (auto it = _meshes.find(mesh_key); it != _meshes.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlMesh>(mesh);
        _meshes.emplace(mesh_key, resource);
        return resource;
    }

    std::shared_ptr<OpenGlShader> OpenGlRenderCache::get_shader(
        const ShaderSource& shader,
        const Uuid& shader_key)
    {
        if (auto it = _shaders.find(shader_key); it != _shaders.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlShader>(shader);
        _shaders.emplace(shader_key, resource);
        return resource;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderCache::get_shader_program(
        const Uuid& shader_id,
        const Shader& shader,
        const GlIdProvider& id_provider)
    {
        auto program_id = shader_id.is_valid() ? shader_id : default_shader.id;
        if (auto it = _shader_programs.find(program_id); it != _shader_programs.end())
        {
            return it->second;
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(shader.sources.size());
        for (size_t source_index = 0U; source_index < shader.sources.size(); ++source_index)
        {
            const ShaderSource& shader_source = shader.sources[source_index];
            Uuid shader_key = id_provider.provide(program_id, static_cast<uint32>(source_index));
            shaders.push_back(get_shader(shader_source, shader_key));
        }

        if (shaders.empty())
        {
            TBX_TRACE_WARNING("OpenGL rendering: shader program has no shaders.");
            return nullptr;
        }

        auto program = std::make_shared<OpenGlShaderProgram>(shaders);
        _shader_programs.emplace(program_id, program);
        return program;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderCache::get_default_texture()
    {
        if (_default_texture)
        {
            return _default_texture;
        }

        Texture default_texture = Texture(
            Size(1, 1),
            TextureWrap::REPEAT,
            TextureFilter::NEAREST,
            TextureFormat::RGBA,
            {255, 255, 255, 255});
        _default_texture = std::make_shared<OpenGlTexture>(default_texture);
        return _default_texture;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderCache::get_texture(
        const Uuid& texture_id,
        const Texture& texture)
    {
        Uuid texture_key = texture_id;
        if (auto it = _textures.find(texture_key); it != _textures.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlTexture>(texture);
        _textures.emplace(texture_key, resource);
        return resource;
    }
}
