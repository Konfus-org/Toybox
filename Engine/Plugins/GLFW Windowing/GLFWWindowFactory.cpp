#include "GLFWWindowFactory.h"
#include "GLFWWindow.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    Tbx::IWindow* GLFWWindowFactory::Create()
    {
        return new GLFWWindow();
    }

    void GLFWWindowFactory::Destroy(Tbx::IWindow* windowToDestroy)
    {
        delete windowToDestroy;
    }

    void GLFWWindowFactory::OnLoad()
    {
        const auto& status = glfwInit();
        TBX_ASSERT(status, "Failed to initialize GLFW!");
    }

    void GLFWWindowFactory::OnUnload()
    {
        glfwTerminate();
    }
}

TBX_REGISTER_PLUGIN(GLFWWindowing::GLFWWindowFactory);