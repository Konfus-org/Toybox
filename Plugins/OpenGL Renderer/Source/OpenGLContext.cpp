#include "OpenGLContext.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <glad/glad.h>

namespace OpenGLRendering
{
    void GLAPIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
        GLsizei length, const GLchar* message, const void* userParam)
    {
        TBX_ERROR("---------------");
        TBX_ERROR("OpenGL Debug Message ({0}): {1}", id, message);

        // Source of the message
        switch (source)
        {
            case GL_DEBUG_SOURCE_API:               TBX_ERROR("Source: API"); break;
            case GL_DEBUG_SOURCE_WINDOW_SYSTEM:     TBX_ERROR("Source: Window System"); break;
            case GL_DEBUG_SOURCE_SHADER_COMPILER:   TBX_ERROR("Source: Shader Compiler"); break;
            case GL_DEBUG_SOURCE_THIRD_PARTY:       TBX_ERROR("Source: Third Party"); break;
            case GL_DEBUG_SOURCE_APPLICATION:       TBX_ERROR("Source: Application"); break;
            case GL_DEBUG_SOURCE_OTHER:             TBX_ERROR("Source: Other"); break;
            default:                                TBX_ERROR("Source: Unknown"); break;
        }

        // Type of the message
        switch (type)
        {
            case GL_DEBUG_TYPE_ERROR:               TBX_ERROR("Type: Error"); break;
            case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: TBX_ERROR("Type: Deprecated Behavior"); break;
            case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  TBX_ERROR("Type: Undefined Behavior"); break;
            case GL_DEBUG_TYPE_PORTABILITY:         TBX_ERROR("Type: Portability"); break;
            case GL_DEBUG_TYPE_PERFORMANCE:         TBX_ERROR("Type: Performance"); break;
            case GL_DEBUG_TYPE_MARKER:              TBX_ERROR("Type: Marker"); break;
            case GL_DEBUG_TYPE_PUSH_GROUP:          TBX_ERROR("Type: Push Group"); break;
            case GL_DEBUG_TYPE_POP_GROUP:           TBX_ERROR("Type: Pop Group"); break;
            case GL_DEBUG_TYPE_OTHER:               TBX_ERROR("Type: Other"); break;
            default:                                TBX_ERROR("Type: Unknown"); break;
        }

        // Severity of the message
        switch (severity)
        {
            case GL_DEBUG_SEVERITY_HIGH:            TBX_ERROR("Severity: High"); break;
            case GL_DEBUG_SEVERITY_MEDIUM:          TBX_ERROR("Severity: Medium"); break;
            case GL_DEBUG_SEVERITY_LOW:             TBX_ERROR("Severity: Low"); break;
            case GL_DEBUG_SEVERITY_NOTIFICATION:    TBX_ERROR("Severity: Notification"); break;
            default:                                TBX_ERROR("Severity: Unknown"); break;
        }

        TBX_ASSERT(false, "Open Gl Error! Check log and callstack for more info.");

        TBX_ERROR("---------------");
    }

    OpenGLContext::~OpenGLContext()
    {
        _windowToRenderTo = nullptr;
    }

    void OpenGLContext::Set(const std::weak_ptr<Tbx::IRenderSurface>& surfaceToRenderInto)
    {
        // Initialize opengl
        auto* window = std::any_cast<GLFWwindow*>(surfaceToRenderInto.lock()->GetNativeWindow());
        TBX_ASSERT(window, "OpenGL graphics context cannot be initialized, native window is invalid!");
        if (_windowToRenderTo == window)
        {
            // We've already been initialized for this window, early out!
            return;
        }

        _windowToRenderTo = window;

#ifdef TBX_PLATFORM_OSX
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
#endif
#ifdef TBX_DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

        glfwMakeContextCurrent(_windowToRenderTo);

        int gladStatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        TBX_ASSERT(gladStatus, "Failed to initialize Glad!");

        TBX_INFO("\n");
        TBX_INFO("OpenGL Info:");
        const std::string& vendorVersion = (const char*)glGetString(GL_VENDOR);
        TBX_INFO("  Vendor: {0}", vendorVersion);
        const std::string& rendererVersion = (const char*)glGetString(GL_RENDERER);
        TBX_INFO("  Renderer: {0}", rendererVersion);
        const std::string& glslVersion = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        TBX_INFO("  GLSL: {0}", glslVersion);
        const std::string& openGLVersion = (const char*)glGetString(GL_VERSION);
        TBX_INFO("  Version: {0}", openGLVersion);
        TBX_INFO("\n");

        TBX_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Tbx requires at least OpenGL version 4.5!");

#ifdef TBX_DEBUG
        glEnable(GL_DEBUG_OUTPUT); // Enable OpenGL debug output
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Ensure messages are synchronous
        glDebugMessageCallback(DebugCallback, nullptr); // Register the callback
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE); // Enable all messages
#endif
    }

    void OpenGLContext::SwapBuffers()
    {
        glfwSwapBuffers(_windowToRenderTo);
    }

    void OpenGLContext::SetSwapInterval(const int& interval) const
    {
        glfwSwapInterval(interval);
    }

    GLFWwindow* OpenGLContext::GetRenderSurface()
    {
        return _windowToRenderTo;
    }
}
