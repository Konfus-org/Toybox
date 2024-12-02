#include "GlfwWindowModule.h"
#include "GlfwWindow.h"

namespace GlfwWindowing
{
    Toybox::Windowing::IWindow* GlfwWindowModule::Create()
    {
        return new GlfwWindow();
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

Toybox::Modules::WindowModule* Load()
{
    return new GlfwWindowing::GlfwWindowModule();
}