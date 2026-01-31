#include "opengl_render_operations.h"
#include "opengl_resource.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/ecs/entities.h"
#include "tbx/graphics/camera.h"
#include "tbx/graphics/material.h"
#include "tbx/graphics/messages.h"
#include "tbx/graphics/model.h"
#include "tbx/graphics/renderer.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <glad/glad.h>
#include <functional>
#include <string>
#include <vector>

namespace tbx::plugins
{
    // Build a model matrix from an entity transform.
    static Mat4 build_model_matrix(const Transform& transform)
    {
        const Mat4 translation = translate(transform.position);
        const Mat4 rotation = quaternion_to_mat4(transform.rotation);
        const Mat4 scaling = scale(transform.scale);
        return translation * rotation * scaling;
    }

    // Compute a cache key for procedural meshes using a stable batch id and mesh index.
    static Uuid make_procedural_mesh_key(const Uuid& batch_id, uint32 mesh_index)
    {
        return Uuid::combine(batch_id, mesh_index);
    }

    // Compute a cache key for asset-backed model meshes.
    static Uuid make_model_mesh_key(const Handle& model_handle, uint32 part_index)
    {
        return Uuid::combine(model_handle.id, part_index);
    }

    static Uuid make_shader_source_key(const ShaderSource& shader)
    {
        const auto hasher = std::hash<std::string>();
        std::string key = shader.source;
        key.append("|");
        key.append(std::to_string(static_cast<int>(shader.type)));
        const auto hashed = static_cast<uint32>(hasher(key));
        return hashed == 0U ? Uuid(1U) : Uuid(hashed);
    }

    static void draw_for_cameras(
        EntityManager& ecs,
        const Size& window_size,
        const std::function<void(const Mat4&)>& draw_call)
    {
        auto cameras = ecs.get_with<Camera, Transform>();

        if (cameras.begin() == cameras.end())
        {
            const float aspect = window_size.get_aspect_ratio();
            const Mat4 default_view = look_at(
                Vec3(0.0f, 0.0f, 5.0f),
                Vec3(0.0f, 0.0f, 0.0f),
                Vec3(0.0f, 1.0f, 0.0f));
            const Mat4 default_projection =
                perspective_projection(degrees_to_radians(60.0f), aspect, 0.1f, 1000.0f);
            const Mat4 view_projection = default_projection * default_view;
            draw_call(view_projection);
            return;
        }

        const float aspect = window_size.get_aspect_ratio();
        for (auto camera_ent : cameras)
        {
            auto& camera = camera_ent.get_component<Camera>();
            camera.set_aspect(aspect);

            const Transform& transform = camera_ent.get_component<Transform>();
            const Mat4 view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);

            draw_call(view_projection);
        }
    }

    static std::shared_ptr<Material> resolve_material(
        AssetManager& asset_manager,
        const Handle& handle)
    {
        static const auto fallback_material = std::make_shared<Material>();

        const Handle resolved = handle.is_valid() ? handle : default_material_handle;
        auto promise = asset_manager.load_async<Material>(resolved);
        if (!promise.asset)
        {
            return fallback_material;
        }
        const AssetUsage usage = asset_manager.get_usage<Material>(resolved);
        if (usage.stream_state != AssetStreamState::Loaded
            && resolved.id != default_material_handle.id)
        {
            auto fallback = asset_manager.load_async<Material>(default_material_handle);
            return fallback.asset ? fallback.asset : fallback_material;
        }
        return promise.asset;
    }

    static std::shared_ptr<OpenGlMesh> get_mesh(
        OpenGlResourceCache& cache,
        const Mesh& mesh,
        const Uuid& mesh_key)
    {
        if (auto it = cache.meshes.find(mesh_key); it != cache.meshes.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlMesh>(mesh);
        cache.meshes.emplace(mesh_key, resource);
        return resource;
    }

    static std::shared_ptr<OpenGlShader> get_shader(
        OpenGlResourceCache& cache,
        const ShaderSource& shader)
    {
        const Uuid shader_key = make_shader_source_key(shader);
        if (auto it = cache.shaders.find(shader_key); it != cache.shaders.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlShader>(shader);
        cache.shaders.emplace(shader_key, resource);
        return resource;
    }

    static std::shared_ptr<OpenGlShaderProgram> get_shader_program(
        OpenGlResourceCache& cache,
        const Handle& handle,
        const Shader& shader)
    {
        const auto program_id = handle.id;
        if (auto it = cache.shader_programs.find(program_id); it != cache.shader_programs.end())
        {
            return it->second;
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(shader.sources.size());
        for (const auto& shader_source : shader.sources)
        {
            shaders.push_back(get_shader(cache, shader_source));
        }

        if (shaders.empty())
        {
            TBX_TRACE_WARNING("OpenGL rendering: shader program has no shaders.");
            return nullptr;
        }

        auto program = std::make_shared<OpenGlShaderProgram>(shaders);
        cache.shader_programs.emplace(program_id, program);
        return program;
    }

    static std::shared_ptr<OpenGlTexture> get_default_texture(OpenGlResourceCache& cache)
    {
        if (cache.default_texture)
        {
            return cache.default_texture;
        }

        Texture default_texture = Texture(
            Size(1, 1),
            TextureWrap::Repeat,
            TextureFilter::Nearest,
            TextureFormat::RGBA,
            {255, 255, 255, 255});
        cache.default_texture = std::make_shared<OpenGlTexture>(default_texture);
        return cache.default_texture;
    }

    static std::shared_ptr<OpenGlTexture> get_texture(
        OpenGlResourceCache& cache,
        const Handle& handle,
        const Texture& texture)
    {
        const Uuid texture_key = handle.id;
        if (auto it = cache.textures.find(texture_key); it != cache.textures.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlTexture>(texture);
        cache.textures.emplace(texture_key, resource);
        return resource;
    }

    static void draw_mesh_with_material(
        AssetManager& asset_manager,
        OpenGlResourceCache& cache,
        const Mesh& mesh,
        const Material& material,
        const Mat4& view_projection,
        const Mat4& model_matrix,
        const Uuid& mesh_key)
    {
        auto mesh_resource = get_mesh(cache, mesh, mesh_key);
        if (!mesh_resource)
        {
            return;
        }

        // Determine which shader passes to render for this material.
        std::vector<Handle> shader_handles = material.shaders;
        if (shader_handles.empty())
        {
            shader_handles.push_back(default_shader_handle);
        }

        for (auto shader_handle : shader_handles)
        {
            // Ensure the shader asset is valid and loaded before drawing.
            if (!shader_handle.is_valid())
            {
                shader_handle = default_shader_handle;
            }

            auto shader_promise = asset_manager.load_async<Shader>(shader_handle);
            if (!shader_promise.asset)
            {
                continue;
            }

            const AssetUsage shader_usage = asset_manager.get_usage<Shader>(shader_handle);
            if (shader_usage.stream_state != AssetStreamState::Loaded
                && shader_handle.id != default_shader_handle.id)
            {
                shader_handle = default_shader_handle;
                shader_promise = asset_manager.load_async<Shader>(shader_handle);
                if (!shader_promise.asset)
                {
                    continue;
                }
            }

            auto program = get_shader_program(cache, shader_handle, *shader_promise.asset);
            if (!program)
            {
                continue;
            }

            GlResourceScope program_scope(*program);
            ShaderUniform view_projection_uniform = {};
            view_projection_uniform.name = "u_view_proj";
            view_projection_uniform.data = view_projection;
            program->upload(view_projection_uniform);

            ShaderUniform model_uniform = {};
            model_uniform.name = "u_model";
            model_uniform.data = model_matrix;
            program->upload(model_uniform);

            // Upload scalar/vector material parameters to matching uniforms.
            for (const auto& parameter : material.parameters)
            {
                ShaderUniform uniform = {};
                uniform.name = "u_" + parameter.name;
                std::visit(
                    [&uniform](const auto& value)
                    {
                        uniform.data = value;
                    },
                    parameter.value);
                program->upload(uniform);
            }

            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(material.textures.size());
            uint32 texture_slot = 0U;

            // Bind textures and upload sampler bindings.
            for (const auto& texture_binding : material.textures)
            {
                std::shared_ptr<OpenGlTexture> texture_resource = {};
                if (texture_binding.handle.is_valid())
                {
                    auto texture_promise =
                        asset_manager.load_async<Texture>(texture_binding.handle);
                    if (texture_promise.asset)
                    {
                        const AssetUsage texture_usage =
                            asset_manager.get_usage<Texture>(texture_binding.handle);
                        if (texture_usage.stream_state == AssetStreamState::Loaded)
                        {
                            texture_resource =
                                get_texture(cache, texture_binding.handle, *texture_promise.asset);
                        }
                    }
                }

                if (!texture_resource)
                {
                    texture_resource = get_default_texture(cache);
                }

                if (texture_resource)
                {
                    texture_resource->set_slot(static_cast<int>(texture_slot));
                    texture_scopes.emplace_back(*texture_resource);
                }

                ShaderUniform texture_uniform = {};
                texture_uniform.name = "u_" + texture_binding.name;
                texture_uniform.data = static_cast<int>(texture_slot);
                program->upload(texture_uniform);
                ++texture_slot;
            }

            // Issue the draw for this mesh and shader pass.
            GlResourceScope mesh_scope(*mesh_resource);
            mesh_resource->draw();
        }
    }

    static void draw_static_models(
        EntityManager& ecs,
        AssetManager& asset_manager,
        OpenGlResourceCache& cache,
        const Mat4& view_projection)
    {
        auto entities = ecs.get_with<Renderer>();
        for (auto& entity : entities)
        {
            const Renderer& renderer = entity.get_component<Renderer>();
            if (!renderer.data)
            {
                continue;
            }

            const auto* static_data = dynamic_cast<const StaticRenderData*>(renderer.data.get());
            if (!static_data)
            {
                continue;
            }

            Mat4 entity_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
            {
                entity_matrix = build_model_matrix(entity.get_component<Transform>());
            }
            if (!static_data->model.is_valid())
            {
                continue;
            }

            auto model_promise = asset_manager.load_async<Model>(static_data->model);
            if (!model_promise.asset)
            {
                continue;
            }

            auto material_asset = resolve_material(asset_manager, static_data->material);

            const Model& model = *model_promise.asset;
            const size_t part_count = model.parts.size();
            if (part_count == 0U)
            {
                for (size_t mesh_index = 0U; mesh_index < model.meshes.size(); ++mesh_index)
                {
                    const auto& mesh = model.meshes[mesh_index];
                    if (mesh.vertices.empty() || mesh.indices.empty())
                    {
                        continue;
                    }
                    const auto mesh_key =
                        make_model_mesh_key(static_data->model, static_cast<uint32>(mesh_index));
                    draw_mesh_with_material(
                        asset_manager,
                        cache,
                        mesh,
                        *material_asset,
                        view_projection,
                        entity_matrix,
                        mesh_key);
                }
                continue;
            }

            std::vector<bool> is_child = std::vector<bool>(part_count, false);
            for (const auto& part : model.parts)
            {
                for (const auto child_index : part.children)
                {
                    if (child_index < part_count)
                    {
                        is_child[child_index] = true;
                    }
                }
            }

            auto draw_part =
                [&](auto&& self, const uint32 part_index, const Mat4& parent_matrix)
            {
                if (part_index >= part_count)
                {
                    return;
                }

                const ModelPart& part = model.parts[part_index];
                const Mat4 part_matrix = parent_matrix * part.transform;

                if (part.mesh_index < model.meshes.size())
                {
                    const Mesh& mesh = model.meshes[part.mesh_index];
                    if (!mesh.vertices.empty() && !mesh.indices.empty())
                    {
                        const auto mesh_key =
                            make_model_mesh_key(static_data->model, part_index);
                        draw_mesh_with_material(
                            asset_manager,
                            cache,
                            mesh,
                            *material_asset,
                            view_projection,
                            part_matrix,
                            mesh_key);
                    }
                }

                for (const auto child_index : part.children)
                {
                    self(self, child_index, part_matrix);
                }
            };

            bool has_root = false;
            for (uint32 part_index = 0U; part_index < part_count; ++part_index)
            {
                if (!is_child[part_index])
                {
                    has_root = true;
                    draw_part(draw_part, part_index, entity_matrix);
                }
            }

            if (!has_root)
            {
                for (uint32 part_index = 0U; part_index < part_count; ++part_index)
                {
                    draw_part(draw_part, part_index, entity_matrix);
                }
            }
        }
    }

    static void draw_procedural_meshes(
        EntityManager& ecs,
        AssetManager& asset_manager,
        OpenGlResourceCache& cache,
        const Mat4& view_projection)
    {
        auto entities = ecs.get_with<Renderer>();
        for (auto& entity : entities)
        {
            const Renderer& renderer = entity.get_component<Renderer>();
            if (!renderer.data)
            {
                continue;
            }

            const auto* procedural_data = dynamic_cast<const ProceduralData*>(renderer.data.get());
            if (!procedural_data)
            {
                continue;
            }

            Mat4 entity_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
            {
                entity_matrix = build_model_matrix(entity.get_component<Transform>());
            }
            const size_t mesh_count = procedural_data->meshes.size();
            for (size_t mesh_index = 0U; mesh_index < mesh_count; ++mesh_index)
            {
                const Mesh& mesh = procedural_data->meshes[mesh_index];
                if (mesh.vertices.empty() || mesh.indices.empty())
                {
                    continue;
                }

                Handle material_handle = {};
                if (mesh_index < procedural_data->materials.size())
                {
                    material_handle = procedural_data->materials[mesh_index];
                }

                auto material_asset = resolve_material(asset_manager, material_handle);
                const auto mesh_key =
                    make_procedural_mesh_key(procedural_data->id, static_cast<uint32>(mesh_index));
                draw_mesh_with_material(
                    asset_manager,
                    cache,
                    mesh,
                    *material_asset,
                    view_projection,
                    entity_matrix,
                    mesh_key);
            }
        }
    }

    BeginFrameOperation::BeginFrameOperation(
        FrameContext& context,
        IMessageDispatcher& dispatcher)
        : _context(context)
        , _dispatcher(dispatcher)
    {
    }

    void BeginFrameOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }

        const auto result =
            _dispatcher.send<WindowMakeCurrentRequest>(_context.window_id);
        if (!result)
        {
            TBX_TRACE_WARNING(
                "OpenGL rendering: failed to make window current: {}",
                result.get_report());
            _context.is_frame_valid = false;
            return;
        }

        const Size& render_resolution = _context.render_resolution;
        glViewport(
            0,
            0,
            static_cast<GLsizei>(render_resolution.width),
            static_cast<GLsizei>(render_resolution.height));
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    CleanCacheOperation::CleanCacheOperation(FrameContext& context)
        : _context(context)
    {
    }

    void CleanCacheOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }
    }

    CacheShadersOperation::CacheShadersOperation(FrameContext& context)
        : _context(context)
    {
    }

    void CacheShadersOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }
    }

    CacheTexturesOperation::CacheTexturesOperation(
        FrameContext& context,
        OpenGlResourceCache& cache)
        : _context(context)
        , _cache(cache)
    {
    }

    void CacheTexturesOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }

        get_default_texture(_cache);
    }

    CacheMeshesOperation::CacheMeshesOperation(FrameContext& context)
        : _context(context)
    {
    }

    void CacheMeshesOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }
    }

    BeginDrawOperation::BeginDrawOperation(FrameContext& context)
        : _context(context)
    {
    }

    void BeginDrawOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }
    }

    DrawModelsOperation::DrawModelsOperation(
        FrameContext& context,
        EntityManager& entity_manager,
        AssetManager& asset_manager,
        OpenGlResourceCache& cache)
        : _context(context)
        , _entity_manager(entity_manager)
        , _asset_manager(asset_manager)
        , _cache(cache)
    {
    }

    void DrawModelsOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }

        draw_for_cameras(
            _entity_manager,
            _context.render_resolution,
            [this](const Mat4& view_projection)
            {
                draw_static_models(_entity_manager, _asset_manager, _cache, view_projection);
            });
    }

    DrawProceduralMeshesOperation::DrawProceduralMeshesOperation(
        FrameContext& context,
        EntityManager& entity_manager,
        AssetManager& asset_manager,
        OpenGlResourceCache& cache)
        : _context(context)
        , _entity_manager(entity_manager)
        , _asset_manager(asset_manager)
        , _cache(cache)
    {
    }

    void DrawProceduralMeshesOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }

        draw_for_cameras(
            _entity_manager,
            _context.render_resolution,
            [this](const Mat4& view_projection)
            {
                draw_procedural_meshes(_entity_manager, _asset_manager, _cache, view_projection);
            });
    }

    EndDrawOperation::EndDrawOperation(FrameContext& context)
        : _context(context)
    {
    }

    void EndDrawOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }
    }

    EndFrameOperation::EndFrameOperation(
        FrameContext& context,
        IMessageDispatcher& dispatcher)
        : _context(context)
        , _dispatcher(dispatcher)
    {
    }

    void EndFrameOperation::execute()
    {
        if (!_context.is_frame_valid)
        {
            return;
        }

        glFlush();
        const auto present_result =
            _dispatcher.send<WindowPresentRequest>(_context.window_id);
        if (!present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: failed to present window: {}",
                present_result.get_report());
        }
    }
}
