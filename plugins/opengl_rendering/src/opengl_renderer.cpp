#include "opengl_renderer.h"
#include "opengl_resources/opengl_buffers.h"
#include "pipeline/opengl_render_pipeline.h"
#include "tbx/debugging/macros.h"
#include <algorithm>
#include <glad/glad.h>
#include <string>
#include <string_view>

namespace opengl_rendering
{
    using namespace tbx;
    void gl_message_callback(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        const GLsizei length,
        const GLchar* message,
        const void* _)
    {
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            case GL_DEBUG_SEVERITY_MEDIUM:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            case GL_DEBUG_SEVERITY_LOW:
            {
                TBX_TRACE_WARNING(
                    "GL debug message (source: {}, type: {}, id: {}, severity: {}) - {}:",
                    source,
                    type,
                    id,
                    severity,
                    message,
                    std::string_view(message, length));
                break;
            }
            default:
                break;
        }
    }

    OpenGlRenderer::OpenGlRenderer(
        const GraphicsProcAddress loader,
        EntityRegistry& entity_registry,
        AssetManager& asset_manager,
        OpenGlContext context)
        : _entity_registry(entity_registry)
        , _context(std::move(context))
    {
        initialize(loader);
    }

    OpenGlRenderer::~OpenGlRenderer() noexcept
    {
        shutdown();
    }

    bool OpenGlRenderer::render() const
    {
        if (const auto make_current_result = _context.make_current(); !make_current_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: failed to make context current: {}",
                make_current_result.get_report());
            return false;
        }

        _render_pipeline->execute({});

        // Step 11: Present the final image to the window backbuffer.
        if (auto present_result = _context.present(); !present_result)
        {
            TBX_TRACE_ERROR(
                "OpenGL rendering: present request failed: {}",
                present_result.get_report());
        }

        return true;
    }

    void OpenGlRenderer::set_viewport_size(const Size& viewport_size)
    {
        if (viewport_size.width == 0 || viewport_size.height == 0)
            return;

        _viewport_size = viewport_size;
    }

    void OpenGlRenderer::set_pending_render_resolution(
        const std::optional<Size>& pending_render_resolution)
    {
        _pending_render_resolution = pending_render_resolution;
    }

    const OpenGlContext& OpenGlRenderer::get_context() const
    {
        return _context;
    }

    void OpenGlRenderer::initialize(const GraphicsProcAddress loader) const
    {
        auto* glad_loader = loader;
        TBX_ASSERT(glad_loader != nullptr, "Context-ready event provided null loader.");
        TBX_ASSERT(
            _context.get_window_id().is_valid(),
            "Renderer requires a valid context window id.");

        if (const auto make_current_result = _context.make_current(); !make_current_result)
        {
            TBX_ASSERT(
                make_current_result,
                "Failed to make context current before GLAD initialization: {}",
                make_current_result.get_report());
        }

        const auto load_result = gladLoadGLLoader(glad_loader);
        TBX_ASSERT(load_result != 0, "Failed to initialize GLAD.");

        TBX_TRACE_INFO("Initializing window {} context.", to_string(_context.get_window_id()));

        const auto major_version = GLVersion.major;
        const auto minor_version = GLVersion.minor;
        TBX_ASSERT(
            major_version > 4 || (major_version == 4 && minor_version >= 5),
            "Requires OpenGL 4.5 or newer.");

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

    void OpenGlRenderer::shutdown()
    {
        _render_pipeline.reset();
        _pending_render_resolution = std::nullopt;
        _viewport_size = {};
        _render_resolution = {};
    }

    void OpenGlRenderer::set_render_resolution(const Size& render_resolution)
    {
        _render_resolution = render_resolution;
    }
}
