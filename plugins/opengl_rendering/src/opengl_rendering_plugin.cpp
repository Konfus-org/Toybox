#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/assets/asset_manager.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <glad/glad.h>
#ifdef USING_SDL3
    #include <SDL3/SDL.h>
#endif
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
    static Uuid make_model_mesh_key(const Uuid& model_id, uint32 part_index)
    {
        return Uuid::combine(model_id, part_index);
    }

    struct MaterialParameterUniformVisitor
    {
        ShaderUniform& uniform;

        template <typename TValue>
        void operator()(const TValue& value) const
        {
            uniform.data = value;
        }
    };

    static void GLAPIENTRY gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar* message,
        const void* user_param)
    {
        (void)source;
        (void)type;
        (void)id;
        (void)length;
        (void)user_param;

        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
                TBX_ASSERT(false, "OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_MEDIUM:
                TBX_TRACE_ERROR("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_LOW:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                TBX_TRACE_INFO("OpenGL callback: {}", message);
                break;
            default:
                TBX_TRACE_WARNING("OpenGL callback: {}", message);
                break;
        }
    }

    void OpenGlRenderingPlugin::on_attach(IPluginHost& host)
    {
        _render_resolution = host.get_settings().resolution.value;
    }

    void OpenGlRenderingPlugin::on_detach() {}

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_gl_ready || _window_sizes.empty())
        {
            return;
        }

        // Track windows that fail to render so their state can be dropped.
        std::vector<Uuid> windows_to_remove = {};
        windows_to_remove.reserve(_window_sizes.size());

        for (const auto& entry : _window_sizes)
        {
            const Uuid window_id = entry.first;
            const Size& window_size = entry.second;
            const Size render_resolution = get_effective_resolution(window_size);

            // Bind the window context before issuing any GL commands.
            const auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (!result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to make window current: {}",
                    result.get_report());
                windows_to_remove.push_back(window_id);
                continue;
            }

            // Configure viewport and clear the frame.
            glViewport(
                0,
                0,
                static_cast<GLsizei>(render_resolution.width),
                static_cast<GLsizei>(render_resolution.height));
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Draw all scene cameras to the current framebuffer.
            draw_models_for_cameras(render_resolution);

            // Present the rendered frame to the window.
            glFlush();
            const auto present_result = send_message<WindowPresentRequest>(window_id);
            if (!present_result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to present window: {}",
                    present_result.get_report());
            }
        }

        // Clean up window state after rendering completes.
        for (const auto& window_id : windows_to_remove)
        {
            remove_window_state(window_id, false);
        }
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            handle_window_ready(*ready_event);
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            handle_window_open_changed(*open_event);
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            handle_window_resized(*size_event);
            return;
        }

        if (auto* resolution_event = handle_property_changed<&AppSettings::resolution>(msg))
        {
            handle_resolution_changed(*resolution_event);
            return;
        }
    }

    void OpenGlRenderingPlugin::handle_window_ready(WindowContextReadyEvent& event)
    {
        bool loaded = false;

        // TODO: Make an abstraction for GL loader selection
#ifdef USING_SDL3
        loaded = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress));
#endif

        if (loaded)
        {
            initialize_opengl();
            _is_gl_ready = true;
        }
        else
            TBX_TRACE_WARNING("Failed to load glad!");

        if (!_is_gl_ready)
        {
            event.state = MessageState::Error;
            event.result.flag_failure("OpenGL rendering: GL loader not initialized.");
            return;
        }

        _window_sizes[event.window] = event.size;

        event.state = MessageState::Handled;
        event.result.flag_success();
    }

    void OpenGlRenderingPlugin::handle_window_open_changed(
        PropertyChangedEvent<Window, bool>& event)
    {
        if (!event.owner)
        {
            return;
        }

        if (event.current)
        {
            return;
        }

        remove_window_state(event.owner->id, true);
    }

    void OpenGlRenderingPlugin::handle_window_resized(PropertyChangedEvent<Window, Size>& event)
    {
        if (!event.owner)
        {
            return;
        }

        const auto window_id = event.owner->id;
        if (_window_sizes.contains(window_id))
        {
            _window_sizes[window_id] = event.current;
        }
    }

    void OpenGlRenderingPlugin::handle_resolution_changed(
        PropertyChangedEvent<AppSettings, Size>& event)
    {
        _render_resolution = event.current;
    }

    void OpenGlRenderingPlugin::initialize_opengl() const
    {
        TBX_TRACE_INFO("OpenGL rendering: initializing OpenGL.");
        TBX_TRACE_INFO("OpenGL rendering: OpenGL info:");

        const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glsl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        TBX_TRACE_INFO("  Vendor: {}", vendor ? vendor : "Unknown");
        TBX_TRACE_INFO("  Renderer: {}", renderer ? renderer : "Unknown");
        TBX_TRACE_INFO("  Version: {}", version ? version : "Unknown");
        TBX_TRACE_INFO("  GLSL: {}", glsl ? glsl : "Unknown");

        TBX_ASSERT(
            GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5),
            "OpenGL rendering: requires OpenGL 4.5 or newer.");

#if defined(TBX_DEBUG)
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageControl(
            GL_DONT_CARE,
            GL_DONT_CARE,
            GL_DEBUG_SEVERITY_NOTIFICATION,
            0,
            nullptr,
            GL_FALSE);
        glDebugMessageCallback(gl_message_callback, nullptr);
#endif

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glDepthFunc(GL_LEQUAL);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    Size OpenGlRenderingPlugin::get_effective_resolution(const Size& window_size) const
    {
        Size resolution = _render_resolution;
        if (resolution.width == 0U || resolution.height == 0U)
        {
            resolution = window_size;
        }

        if (resolution.width == 0U)
        {
            resolution.width = 1U;
        }

        if (resolution.height == 0U)
        {
            resolution.height = 1U;
        }

        return resolution;
    }

    void OpenGlRenderingPlugin::remove_window_state(const Uuid& window_id, const bool try_release)
    {
        _window_sizes.erase(window_id);
    }

    void OpenGlRenderingPlugin::draw_models(const Mat4& view_projection)
    {
        auto& ecs = get_host().get_entity_manager();
        auto& asset_manager = get_host().get_asset_manager();

        static const auto fallback_material = std::make_shared<Material>();

        // Step 1: Gather renderable entities.
        auto entities = ecs.get_with<Renderer>();
        for (auto& entity : entities)
        {
            const Renderer& renderer = entity.get_component<Renderer>();
            if (!renderer.data)
            {
                continue;
            }

            // Step 2: Compute the entity model matrix (identity if no transform).
            Mat4 entity_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
            {
                entity_matrix = build_model_matrix(entity.get_component<Transform>());
            }

            if (auto* static_data = dynamic_cast<const StaticRenderData*>(renderer.data.get()))
            {
                // Step 3: Draw meshes from a static model asset.
                draw_static_model(
                    asset_manager,
                    *static_data,
                    entity_matrix,
                    view_projection,
                    fallback_material);
                continue;
            }

            if (auto* procedural_data = dynamic_cast<const ProceduralData*>(renderer.data.get()))
            {
                // Step 4: Draw procedurally supplied meshes.
                draw_procedural_meshes(
                    asset_manager,
                    *procedural_data,
                    entity_matrix,
                    view_projection,
                    fallback_material);
            }
        }
    }

    std::shared_ptr<Material> OpenGlRenderingPlugin::resolve_material_asset(
        AssetManager& asset_manager,
        const Handle& handle,
        const std::shared_ptr<Material>& fallback_material) const
    {
        // Step 1: Prefer the explicit handle, fall back to the default material.
        const Handle resolved = handle.is_valid() ? handle : default_material_handle;

        // Step 2: Attempt to load the requested material asset.
        auto promise = asset_manager.load_async<Material>(resolved);
        if (!promise.asset)
        {
            return fallback_material;
        }

        // Step 3: If the asset is still streaming, use the default material instead.
        const AssetUsage usage = asset_manager.get_usage<Material>(resolved);
        if (usage.stream_state != AssetStreamState::Loaded
            && resolved.id != default_material_handle.id)
        {
            auto fallback = asset_manager.load_async<Material>(default_material_handle);
            return fallback.asset ? fallback.asset : fallback_material;
        }

        return promise.asset;
    }

    void OpenGlRenderingPlugin::draw_mesh_with_material(
        AssetManager& asset_manager,
        const Mesh& mesh,
        const Material& material,
        const Mat4& model_matrix,
        const Mat4& view_projection,
        const Uuid& mesh_key)
    {
        // Step 1: Fetch or create the mesh resource for this cache key.
        auto mesh_resource = get_mesh(mesh, mesh_key);
        if (!mesh_resource)
        {
            return;
        }

        // Step 2: Determine which shader passes to render.
        std::vector<Handle> shader_handles = material.shaders;
        if (shader_handles.empty())
        {
            shader_handles.push_back(default_shader_handle);
        }

        for (auto shader_handle : shader_handles)
        {
            // Step 3: Validate and load the shader asset.
            if (!shader_handle.is_valid())
            {
                shader_handle = default_shader_handle;
            }

            auto shader_promise = asset_manager.load<Shader>(shader_handle);
            if (!shader_promise)
            {
                continue;
            }

            const AssetUsage shader_usage = asset_manager.get_usage<Shader>(shader_handle);
            if (shader_usage.stream_state != AssetStreamState::Loaded
                && shader_handle.id != default_shader_handle.id)
            {
                shader_handle = default_shader_handle;
                shader_promise = asset_manager.load<Shader>(shader_handle);
                if (!shader_promise)
                {
                    continue;
                }
            }

            const Uuid shader_id = asset_manager.resolve_asset_id(shader_handle);
            auto program = get_shader_program(shader_id, *shader_promise);
            if (!program)
            {
                continue;
            }

            // Step 4: Bind the shader program and upload view/model matrices.
            GlResourceScope program_scope(*program);

            ShaderUniform view_projection_uniform = {};
            view_projection_uniform.name = "u_view_proj";
            view_projection_uniform.data = view_projection;
            program->upload(view_projection_uniform);

            ShaderUniform model_uniform = {};
            model_uniform.name = "u_model";
            model_uniform.data = model_matrix;
            program->upload(model_uniform);

            // Step 5: Upload scalar/vector material parameters.
            for (const auto& parameter : material.parameters)
            {
                ShaderUniform uniform = {};
                uniform.name = "u_" + parameter.name;
                MaterialParameterUniformVisitor visitor{uniform};
                std::visit(visitor, parameter.value);
                program->upload(uniform);
            }

            // Step 6: Bind textures and upload sampler slots.
            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(material.textures.size());
            uint32 texture_slot = 0U;

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
                            const Uuid texture_id =
                                asset_manager.resolve_asset_id(texture_binding.handle);
                            texture_resource = get_texture(texture_id, *texture_promise.asset);
                        }
                    }
                }

                if (!texture_resource)
                {
                    texture_resource = get_default_texture();
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

            // Step 7: Bind the mesh and issue the draw call.
            GlResourceScope mesh_scope(*mesh_resource);
            mesh_resource->draw();
        }
    }

    void OpenGlRenderingPlugin::draw_static_model(
        AssetManager& asset_manager,
        const StaticRenderData& static_data,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& fallback_material)
    {
        // Step 1: Validate the model handle and load the asset.
        if (!static_data.model.is_valid())
        {
            return;
        }

        auto model_promise = asset_manager.load_async<Model>(static_data.model);
        if (!model_promise.asset)
        {
            return;
        }

        // Step 2: Resolve the material asset for this model.
        auto material_asset =
            resolve_material_asset(asset_manager, static_data.material, fallback_material);

        const Uuid model_id = asset_manager.resolve_asset_id(static_data.model);
        const Model& model = *model_promise.asset;
        const size_t part_count = model.parts.size();

        // Step 3: Draw meshes directly when the model has no part hierarchy.
        if (part_count == 0U)
        {
            for (size_t mesh_index = 0U; mesh_index < model.meshes.size(); ++mesh_index)
            {
                const Mesh& mesh = model.meshes[mesh_index];
                if (mesh.vertices.empty() || mesh.indices.empty())
                {
                    continue;
                }

                const Uuid mesh_key =
                    make_model_mesh_key(model_id, static_cast<uint32>(mesh_index));
                draw_mesh_with_material(
                    asset_manager,
                    mesh,
                    *material_asset,
                    entity_matrix,
                    view_projection,
                    mesh_key);
            }
            return;
        }

        // Step 4: Traverse the part hierarchy so transforms are applied in order.
        draw_model_parts(
            asset_manager,
            model,
            model_id,
            entity_matrix,
            view_projection,
            material_asset);
    }

    void OpenGlRenderingPlugin::draw_model_parts(
        AssetManager& asset_manager,
        const Model& model,
        const Uuid& model_id,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& material_asset)
    {
        const size_t part_count = model.parts.size();
        std::vector<bool> is_child = std::vector<bool>(part_count, false);

        // Step 4a: Mark parts that are referenced as children.
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

        bool has_root = false;
        for (uint32 part_index = 0U; part_index < part_count; ++part_index)
        {
            if (!is_child[part_index])
            {
                has_root = true;
                draw_model_part_recursive(
                    asset_manager,
                    model,
                    model_id,
                    entity_matrix,
                    view_projection,
                    material_asset,
                    part_index);
            }
        }

        // Step 4b: If no roots were found, draw all parts from the entity transform.
        if (!has_root)
        {
            for (uint32 part_index = 0U; part_index < part_count; ++part_index)
            {
                draw_model_part_recursive(
                    asset_manager,
                    model,
                    model_id,
                    entity_matrix,
                    view_projection,
                    material_asset,
                    part_index);
            }
        }
    }

    void OpenGlRenderingPlugin::draw_model_part_recursive(
        AssetManager& asset_manager,
        const Model& model,
        const Uuid& model_id,
        const Mat4& parent_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& material_asset,
        uint32 part_index)
    {
        if (part_index >= model.parts.size())
        {
            return;
        }

        const ModelPart& part = model.parts[part_index];

        // Step 4c: Combine the parent transform with this part's local transform.
        const Mat4 part_matrix = parent_matrix * part.transform;

        // Step 4d: Draw the mesh bound to this part, if any.
        if (part.mesh_index < model.meshes.size())
        {
            const Mesh& mesh = model.meshes[part.mesh_index];
            if (!mesh.vertices.empty() && !mesh.indices.empty())
            {
                const Uuid mesh_key =
                    make_model_mesh_key(model_id, part.mesh_index);
                draw_mesh_with_material(
                    asset_manager,
                    mesh,
                    *material_asset,
                    part_matrix,
                    view_projection,
                    mesh_key);
            }
        }

        // Step 4e: Recurse into child parts.
        for (const auto child_index : part.children)
        {
            draw_model_part_recursive(
                asset_manager,
                model,
                model_id,
                part_matrix,
                view_projection,
                material_asset,
                child_index);
        }
    }

    void OpenGlRenderingPlugin::draw_procedural_meshes(
        AssetManager& asset_manager,
        const ProceduralData& procedural_data,
        const Mat4& entity_matrix,
        const Mat4& view_projection,
        const std::shared_ptr<Material>& fallback_material)
    {
        // Step 1: Iterate the procedural mesh list.
        const size_t mesh_count = procedural_data.meshes.size();
        for (size_t mesh_index = 0U; mesh_index < mesh_count; ++mesh_index)
        {
            const Mesh& mesh = procedural_data.meshes[mesh_index];
            if (mesh.vertices.empty() || mesh.indices.empty())
            {
                continue;
            }

            // Step 2: Resolve a material for this mesh index (or use default).
            Handle material_handle = {};
            if (mesh_index < procedural_data.materials.size())
            {
                material_handle = procedural_data.materials[mesh_index];
            }

            auto material_asset =
                resolve_material_asset(asset_manager, material_handle, fallback_material);

            // Step 3: Draw the mesh with a stable cache key.
            const Uuid mesh_key =
                make_procedural_mesh_key(procedural_data.id, static_cast<uint32>(mesh_index));
            draw_mesh_with_material(
                asset_manager,
                mesh,
                *material_asset,
                entity_matrix,
                view_projection,
                mesh_key);
        }
    }

    void OpenGlRenderingPlugin::draw_models_for_cameras(const Size& window_size)
    {
        auto& ecs = get_host().get_entity_manager();
        auto cameras = ecs.get_with<Camera, Transform>();

        if (cameras.begin() == cameras.end())
        {
            const float aspect = window_size.get_aspect_ratio();
            const Mat4 default_view =
                look_at(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
            const Mat4 default_projection =
                perspective_projection(degrees_to_radians(60.0f), aspect, 0.1f, 1000.0f);
            const Mat4 view_projection = default_projection * default_view;
            draw_models(view_projection);
            return;
        }

        const float aspect = window_size.get_aspect_ratio();
        for (auto cameraEnt : cameras)
        {
            auto& camera = cameraEnt.get_component<Camera>();
            camera.set_aspect(aspect);

            const Transform& transform = cameraEnt.get_component<Transform>();
            const Mat4 view_projection =
                camera.get_view_projection_matrix(transform.position, transform.rotation);

            draw_models(view_projection);
        }
    }

    std::shared_ptr<OpenGlMesh> OpenGlRenderingPlugin::get_mesh(
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

    static Uuid make_shader_source_key(const Uuid& shader_id, uint32 source_index)
    {
        const Uuid resolved =
            shader_id.is_valid() ? shader_id : default_shader_handle.id;
        return Uuid::combine(resolved, source_index);
    }

    std::shared_ptr<OpenGlShader> OpenGlRenderingPlugin::get_shader(
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

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderingPlugin::get_shader_program(
        const Uuid& shader_id,
        const Shader& shader)
    {
        const auto program_id = shader_id.is_valid() ? shader_id : default_shader_handle.id;
        if (auto it = _shader_programs.find(program_id); it != _shader_programs.end())
        {
            return it->second;
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(shader.sources.size());
        for (size_t source_index = 0U; source_index < shader.sources.size(); ++source_index)
        {
            const ShaderSource& shader_source = shader.sources[source_index];
            const Uuid shader_key =
                make_shader_source_key(program_id, static_cast<uint32>(source_index));
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

    std::shared_ptr<OpenGlTexture> OpenGlRenderingPlugin::get_default_texture()
    {
        if (_default_texture)
        {
            return _default_texture;
        }

        Texture default_texture = Texture(
            Size(1, 1),
            TextureWrap::Repeat,
            TextureFilter::Nearest,
            TextureFormat::RGBA,
            {255, 255, 255, 255});
        _default_texture = std::make_shared<OpenGlTexture>(default_texture);
        return _default_texture;
    }

    std::shared_ptr<OpenGlTexture> OpenGlRenderingPlugin::get_texture(
        const Uuid& texture_id,
        const Texture& texture)
    {
        const Uuid texture_key = texture_id;
        if (auto it = _textures.find(texture_key); it != _textures.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlTexture>(texture);
        _textures.emplace(texture_key, resource);
        return resource;
    }
}
