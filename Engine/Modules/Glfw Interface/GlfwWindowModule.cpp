#include "GlfwWindowModule.h"
#include "GlfwWindow.h"

namespace GlfwWindowing
{
    Toybox::IWindow* GlfwWindowModule::OpenNewWindow(const std::string& name, Toybox::WindowMode mode, Toybox::Size size)
    {
        auto* glfwWindow = new GlfwWindow();
        glfwWindow->SetTitle(name);
        glfwWindow->SetSize(size);
        glfwWindow->Open(mode);
        return glfwWindow;
    }

    void GlfwWindowModule::DestroyWindow(Toybox::IWindow* windowToDestroy)
    {
        delete windowToDestroy;
    }

    const std::string GlfwWindowModule::GetName() const
    {
        return "Glfw Windowing";
    }

    const std::string GlfwWindowModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    const int GlfwWindowModule::GetVersion() const
    {
        return 0;
    }
}
