#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include "tbx/math/trig.h"
#include <glad/glad.h>
#ifdef USING_SDL3
    #include <SDL3/SDL.h>
#endif
#include <vector>

namespace tbx::plugins
{
    static Mat4 build_model_matrix(const Transform& transform)
    {
        const Mat4 translation = translate(transform.position);
        const Mat4 rotation = quaternion_to_mat4(transform.rotation);
        const Mat4 scaling = scale(transform.scale);
        return translation * rotation * scaling;
    }

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

    void OpenGlRenderingPlugin::on_attach(Application& host)
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

        std::vector<Uuid> windows_to_remove = {};
        windows_to_remove.reserve(_window_sizes.size());

        for (const auto& entry : _window_sizes)
        {
            const Uuid window_id = entry.first;
            const Size& window_size = entry.second;
            const Size render_resolution = get_effective_resolution(window_size);

            const auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (!result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to make window current: {}",
                    result.get_report());
                windows_to_remove.push_back(window_id);
                continue;
            }

            // 1. Set viewport and clear color first
            glViewport(
                0,
                0,
                static_cast<GLsizei>(render_resolution.width),
                static_cast<GLsizei>(render_resolution.height));
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Fixed 255 to 1.0f

            // 2. Then perform the clear
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // 3. Draw scene to framebuffer
            draw_models_for_cameras(render_resolution);

            // 4. Flush and present
            glFlush();
            const auto present_result = send_message<WindowPresentRequest>(window_id);
            if (!present_result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to present window: {}",
                    present_result.get_report());
            }
        }

        for (const auto& window_id : windows_to_remove)
        {
            remove_window_state(window_id, false);
        }
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (on_message(
                msg,
                [this](WindowContextReadyEvent& event)
                {
                    handle_window_ready(event);
                }))
        {
            return;
        }

        if (on_property_changed(
                msg,
                &Window::is_open,
                [this](PropertyChangedEvent<Window, bool>& event)
                {
                    handle_window_open_changed(event);
                }))
        {
            return;
        }

        if (on_property_changed(
                msg,
                &Window::size,
                [this](PropertyChangedEvent<Window, Size>& event)
                {
                    handle_window_resized(event);
                }))
        {
            return;
        }

        if (on_property_changed(
                msg,
                &AppSettings::resolution,
                [this](PropertyChangedEvent<AppSettings, Size>& event)
                {
                    handle_resolution_changed(event);
                }))
        {
            return;
        }
    }

    void OpenGlRenderingPlugin::handle_window_ready(WindowContextReadyEvent& event)
    {
        bool loaded = false;

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
        auto& ecs = get_host().get_ecs();
        const auto draw_mesh_with_material =
            [&](const Mesh& mesh, const Material& material, const Mat4& model_matrix)
        {
            auto mesh_resource = get_mesh(mesh);
            auto program = get_shader_program(material);
            if (!mesh_resource || !program)
            {
                return;
            }

            GlResourceScope program_scope(*program);
            ShaderUniform view_projection_uniform = {};
            view_projection_uniform.name = "view_proj_uniform";
            view_projection_uniform.data = view_projection;
            program->upload(view_projection_uniform);

            ShaderUniform model_uniform = {};
            model_uniform.name = "model_uniform";
            model_uniform.data = model_matrix;
            program->upload(model_uniform);

            ShaderUniform color_uniform = {};
            color_uniform.name = "color_uniform";
            color_uniform.data = material.color;
            program->upload(color_uniform);

            ShaderUniform texture_uniform = {};
            texture_uniform.name = "texture_uniform";
            texture_uniform.data = 0;
            program->upload(texture_uniform);

            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(material.textures.size() + 1);
            uint32 slot = 0;
            if (material.textures.empty())
            {
                auto resource = get_default_texture();
                if (resource)
                {
                    resource->set_slot(slot);
                    texture_scopes.emplace_back(*resource);
                    slot += 1;
                }
            }

            for (const auto& texture : material.textures)
            {
                if (!texture)
                {
                    continue;
                }
                auto resource = get_texture(*texture);
                if (!resource)
                {
                    continue;
                }
                resource->set_slot(slot);
                texture_scopes.emplace_back(*resource);
                slot += 1;
            }

            GlResourceScope mesh_scope(*mesh_resource);
            mesh_resource->draw();
        };

        auto entities = ecs.get_entities_with<Model>();
        for (auto& entity : entities)
        {
            const Model& model = entity.get_component<Model>();

            Mat4 model_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
                model_matrix = build_model_matrix(entity.get_component<Transform>());

            draw_mesh_with_material(model.mesh, model.material, model_matrix);
        }

        static const Material fallback_material = []()
        {
            Material material = {};
            material.color = RgbaColor::magenta;
            return material;
        }();

        auto mesh_entities = ecs.get_entities_with<Mesh>();
        for (auto& entity : mesh_entities)
        {
            if (entity.has_component<Model>())
            {
                continue;
            }

            const Mesh& mesh = entity.get_component<Mesh>();
            Mat4 model_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
                model_matrix = build_model_matrix(entity.get_component<Transform>());

            const Material* material = nullptr;
            if (entity.has_component<Material>())
            {
                material = &entity.get_component<Material>();
            }
            else
            {
                const Uuid entity_id = entity.get_id();
                const bool is_new_warning =
                    _mesh_entities_missing_material.emplace(entity_id).second;
                if (is_new_warning)
                {
                    TBX_TRACE_WARNING(
                        "OpenGL rendering: mesh entity {} has no material component; rendering "
                        "with bright pink.",
                        to_string(entity_id));
                }
                material = &fallback_material;
            }

            draw_mesh_with_material(mesh, *material, model_matrix);
        }
    }

    void OpenGlRenderingPlugin::draw_models_for_cameras(const Size& window_size)
    {
        auto& ecs = get_host().get_ecs();
        auto cameras = ecs.get_entities_with<Camera, Transform>();

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

    std::shared_ptr<OpenGlMesh> OpenGlRenderingPlugin::get_mesh(const Mesh& mesh)
    {
        if (auto it = _meshes.find(mesh.id); it != _meshes.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlMesh>(mesh);
        _meshes.emplace(mesh.id, resource);
        return resource;
    }

    std::shared_ptr<OpenGlShader> OpenGlRenderingPlugin::get_shader(const Shader& shader)
    {
        if (auto it = _shaders.find(shader.id); it != _shaders.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlShader>(shader);
        _shaders.emplace(shader.id, resource);
        return resource;
    }

    std::shared_ptr<OpenGlShaderProgram> OpenGlRenderingPlugin::get_shader_program(
        const Material& material)
    {
        const auto program_id = material.shader_program.id;
        if (auto it = _shader_programs.find(program_id); it != _shader_programs.end())
        {
            return it->second;
        }

        std::vector<std::shared_ptr<OpenGlShader>> shaders = {};
        shaders.reserve(material.shader_program.shaders.size());
        for (const auto& shader : material.shader_program.shaders)
        {
            if (!shader)
            {
                continue;
            }
            shaders.push_back(get_shader(*shader));
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

    std::shared_ptr<OpenGlTexture> OpenGlRenderingPlugin::get_texture(const Texture& texture)
    {
        if (auto it = _textures.find(texture.id); it != _textures.end())
        {
            return it->second;
        }

        auto resource = std::make_shared<OpenGlTexture>(texture);
        _textures.emplace(texture.id, resource);
        return resource;
    }
}
