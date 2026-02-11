#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/camera.h"
#include "tbx/messages/observable.h"
#include <glad/glad.h>

namespace tbx::plugins
{
    static void GLAPIENTRY gl_message_callback(
        GLenum,
        GLenum,
        GLuint,
        GLenum severity,
        GLsizei,
        const GLchar* message,
        const void*)
    {
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
        _render_pipeline = std::make_unique<OpenGlRenderPipeline>(host.get_asset_manager());
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        _is_context_ready = false;
        _render_pipeline.reset();
    }

    void OpenGlRenderingPlugin::on_update(const DeltaTime&)
    {
        if (!_is_context_ready || !_render_pipeline)
            return;

        send_message<WindowMakeCurrentRequest>(_window_id);

        auto camera_view = OpenGlCameraView {};
        bool did_find_camera = false;
        auto& registry = get_host().get_entity_registry();
        registry.for_each_with<Camera>(
            [&camera_view, &did_find_camera](Entity& entity)
            {
                if (did_find_camera)
                    return;

                camera_view.camera_entity = entity;
                did_find_camera = true;
            });

        if (!did_find_camera)
            return;

        registry.for_each_with<Renderer, StaticMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_static_entities.push_back(entity);
            });

        registry.for_each_with<Renderer, DynamicMesh>(
            [&camera_view](Entity& entity)
            {
                camera_view.in_view_dynamic_entities.push_back(entity);
            });

        auto frame_context = OpenGlRenderFrameContext {
            .camera_view = camera_view,
            .render_resolution = _render_resolution,
            .viewport_size = _viewport_size,
            .render_target = &_framebuffer,
        };

        _render_pipeline->execute(frame_context);
        send_message<WindowPresentRequest>(_window_id);
    }

    void OpenGlRenderingPlugin::on_recieve_message(Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            _window_id = ready_event->window;
            if (!_render_pipeline)
                _render_pipeline =
                    std::make_unique<OpenGlRenderPipeline>(get_host().get_asset_manager());

            auto* loader = reinterpret_cast<GLADloadproc>(ready_event->get_proc_address);
            TBX_ASSERT(
                loader != nullptr,
                "OpenGL rendering: context-ready event provided null loader.");
            const auto load_result = gladLoadGLLoader(loader);
            TBX_ASSERT(load_result != 0, "OpenGL rendering: failed to initialize GLAD.");

            initialize_opengl();
            set_viewport_size(ready_event->size);
            set_render_resolution(ready_event->size);
            _is_context_ready = true;
            return;
        }

        if (auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (size_event->owner && size_event->owner->id == _window_id)
            {
                set_viewport_size(size_event->current);
                set_render_resolution(size_event->current);
            }
            return;
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
        glClearColor(0.07f, 0.08f, 0.11f, 1.0f);
    }

    void OpenGlRenderingPlugin::set_viewport_size(const Size& viewport_size)
    {
        if (viewport_size.width == 0 || viewport_size.height == 0)
            return;

        _viewport_size = viewport_size;
    }

    void OpenGlRenderingPlugin::set_render_resolution(const Size& render_resolution)
    {
        if (render_resolution.width == 0 || render_resolution.height == 0)
            return;

        _render_resolution = render_resolution;
        _framebuffer.set_size(render_resolution);
    }
}
