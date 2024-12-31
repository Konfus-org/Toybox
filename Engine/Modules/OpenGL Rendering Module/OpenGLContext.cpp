#include "OpenGLContext.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace OpenGLRendering
{
    OpenGLContext::OpenGLContext(const std::weak_ptr<Toybox::IWindow>& windowToRenderInto)
    {
        // Initialize opengl
        auto* window = std::any_cast<GLFWwindow*>(windowToRenderInto.lock()->GetNativeWindow());
        TBX_ASSERT(window, "OpenGL graphics context cannot be initialized, native window is invalid!");
        _windowToRenderTo = window;

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

        TBX_ASSERT(GLVersion.major > 4 || (GLVersion.major == 4 && GLVersion.minor >= 5), "Toybox requires at least OpenGL version 4.5!");
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
