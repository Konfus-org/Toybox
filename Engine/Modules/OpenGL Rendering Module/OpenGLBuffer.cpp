#include "OpenGLBuffer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    OpenGLBuffer::OpenGLBuffer(const std::weak_ptr<Toybox::IWindow>& windowToRenderInto)
    {
        // Initialize opengl
        auto* window = std::any_cast<GLFWwindow*>(windowToRenderInto.lock()->GetNativeWindow());
        TBX_ASSERT(window, "OpenGL graphics context cannot be initialized, native window is invalid!");
        _windowToRenderTo = window;

        glfwMakeContextCurrent(_windowToRenderTo);
        int gladStatus = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        TBX_ASSERT(gladStatus, "Failed to initialize Glad!");

        ////TBX_INFO("OpenGL Info:");
        ////const auto& vendorVersion = glGetString(GL_VENDOR);
        ////TBX_INFO("  Vendor: {0}", vendorVersion);
        ////const auto& openGLVersion = glGetString(GL_VERSION);
        ////TBX_INFO("  Version: {0}", openGLVersion);
        ////const auto& rendererVersion = glGetString(GL_RENDERER);
        ////TBX_INFO("  Renderer: {0}", rendererVersion);

        ////TBX_INFO("GLSL Info:");
        ////const auto& GLSLVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
        ////TBX_INFO("  Version: {0}", GLSLVersion);

        TBX_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Toybox requires at least OpenGL version 4.5!");
    }

    void OpenGLBuffer::Swap()
    {
        glfwSwapBuffers(_windowToRenderTo);
    }

    void OpenGLBuffer::SetSwapInterval(const int& interval) const
    {
        glfwSwapInterval(interval);
    }
}
