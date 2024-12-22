#include "GlfwWindowModule.h"
#include "GlfwWindow.h"
#include <GLFW/glfw3.h>

namespace GlfwWindowing
{
    Toybox::IWindow* GlfwWindowModule::Create()
    {
        return new GlfwWindow();
    }

    void GlfwWindowModule::Destroy(Toybox::IWindow* windowToDestroy)
    {
        delete windowToDestroy;
    }

    std::string GlfwWindowModule::GetName() const
    {
        return "Glfw Windowing";
    }

    std::string GlfwWindowModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GlfwWindowModule::GetVersion() const
    {
        return 0;
    }
}

Toybox::Module* Load()
{
    const bool& success = glfwInit();
    TBX_ASSERT(success, "Failed to initialize glfw!");
    return new GlfwWindowing::GlfwWindowModule();
}

void Unload(Toybox::Module* moduleToUnload)
{
    delete moduleToUnload;
    glfwTerminate();
}