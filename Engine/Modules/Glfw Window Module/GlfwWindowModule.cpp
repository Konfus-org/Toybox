#include "GLFWWindowModule.h"
#include "GLFWWindow.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    Tbx::IWindow* GLFWWindowModule::Create()
    {
        return new GLFWWindow();
    }

    void GLFWWindowModule::Destroy(Tbx::IWindow* windowToDestroy)
    {
        delete windowToDestroy;
    }

    std::string GLFWWindowModule::GetName() const
    {
        return "GLFW Windowing";
    }

    std::string GLFWWindowModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GLFWWindowModule::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    const bool& success = glfwInit();
    TBX_ASSERT(success, "Failed to initialize glfw!");
    return new GLFWWindowing::GLFWWindowModule();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
    glfwTerminate();
}