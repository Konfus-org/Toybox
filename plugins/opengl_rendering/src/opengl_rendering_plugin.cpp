#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/math/matrices.h"
#include "tbx/math/transform.h"
#include <glad/glad.h>
#include <vector>

namespace tbx::plugins::openglrendering
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

    void OpenGlRenderingPlugin::on_attach(Application&) {}

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_gl_ready || _window_sizes.empty())
        {
            return;
        }

        for (const auto& entry : _window_sizes)
        {
            const Uuid window_id = entry.first;
            const Size& window_size = entry.second;

            const auto result = send_message<WindowMakeCurrentRequest>(window_id);
            if (!result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to make window current: {}",
                    result.get_report());
                continue;
            }

            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glClearDepth(1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glViewport(
                0,
                0,
                static_cast<GLsizei>(window_size.width),
                static_cast<GLsizei>(window_size.height));

            draw_models_for_cameras(window_size);

            glFlush();

            const auto present_result = send_message<WindowPresentRequest>(window_id);
            if (!present_result)
            {
                TBX_TRACE_ERROR(
                    "OpenGL rendering: failed to present window: {}",
                    present_result.get_report());
            }
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
                &Window::size,
                [this](PropertyChangedEvent<Window, Size>& event)
                {
                    handle_window_resized(event);
                }))
        {
            return;
        }
    }

    void OpenGlRenderingPlugin::handle_window_ready(WindowContextReadyEvent& event)
    {
        if (!event.get_proc_address)
        {
            event.state = MessageState::Error;
            event.result.flag_failure("OpenGL rendering: missing window proc address loader.");
            return;
        }

        if (!_is_gl_ready)
        {
            const auto loaded = gladLoadGLLoader(event.get_proc_address);
            if (!loaded)
            {
                event.state = MessageState::Error;
                event.result.flag_failure("OpenGL rendering: failed to load GL functions.");
                return;
            }

            initialize_opengl();
            _is_gl_ready = true;
        }

        _window_sizes[event.window] = event.size;

        event.state = MessageState::Handled;
        event.result.flag_success();
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

    void OpenGlRenderingPlugin::draw_models(const Mat4& view_projection)
    {
        auto& ecs = get_host().get_ecs();
        auto entities = ecs.get_entities_with<Model>();

        for (auto& entity : entities)
        {
            const Model& model = entity.get_component<Model>();

            Mat4 model_matrix = Mat4(1.0f);
            if (entity.has_component<Transform>())
                model_matrix = build_model_matrix(entity.get_component<Transform>());

            auto mesh = get_mesh(model.mesh);
            auto program = get_shader_program(model.material);
            if (!mesh || !program)
            {
                continue;
            }

            GlResourceScope program_scope(*program);
            ShaderUniform view_projection_uniform = {};
            view_projection_uniform.name = "view_projection_uniform";
            view_projection_uniform.data = view_projection;
            program->upload(view_projection_uniform);

            ShaderUniform model_uniform = {};
            model_uniform.name = "u_model";
            model_uniform.data = model_matrix;
            program->upload(model_uniform);

            std::vector<GlResourceScope> texture_scopes = {};
            texture_scopes.reserve(model.material.textures.size());
            uint32 slot = 0;
            for (const auto& texture : model.material.textures)
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

            GlResourceScope mesh_scope(*mesh);
            mesh->draw();
        }
    }

    void OpenGlRenderingPlugin::draw_models_for_cameras(const Size& window_size)
    {
        auto& ecs = get_host().get_ecs();
        auto entities = ecs.get_entities_with<Camera, Transform>();

        if (entities.begin() == entities.end())
        {
            draw_models(Mat4(1.0f));
            return;
        }

        const float aspect = window_size.get_aspect_ratio();
        for (auto entity : entities)
        {
            Camera& camera = entity.get_component<Camera>();
            camera.set_aspect(aspect);

            const Transform& transform = entity.get_component<Transform>();
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
