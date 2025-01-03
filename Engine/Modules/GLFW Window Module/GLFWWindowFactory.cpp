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

    std::string GLFWWindowFactory::GetName() const
    {
        return "GLFW Windowing";
    }

    std::string GLFWWindowFactory::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GLFWWindowFactory::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    const bool& success = glfwInit();
    TBX_ASSERT(success, "Failed to initialize glfw!");
    return new GLFWWindowing::GLFWWindowFactory();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
    glfwTerminate();
}