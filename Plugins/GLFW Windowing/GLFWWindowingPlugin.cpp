#include "GLFWWindowingPlugin.h"
#include "GLFWWindowFactory.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    Tbx::IWindowFactory* GLFWWindowing::GLFWWindowingPlugin::Provide()
    {
        return new GLFWWindowFactory();
    }

    void GLFWWindowingPlugin::Destroy(Tbx::IWindowFactory* toDestroy)
    {
        delete toDestroy;
    }

    void GLFWWindowingPlugin::OnLoad()
    {
        const auto& status = glfwInit();
        TBX_ASSERT(status, "Failed to initialize GLFW!");
    }

    void GLFWWindowingPlugin::OnUnload()
    {
        glfwTerminate();
    }
}

TBX_REGISTER_PLUGIN(GLFWWindowing::GLFWWindowingPlugin);