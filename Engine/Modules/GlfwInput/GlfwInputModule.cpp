#include "GlfwInputModule.h"
#include "GlfwInputHandler.h"

namespace GlfwInput
{
    Toybox::Input::IInputHandler* GlfwInputModule::Create()
    {
        return new GlfwInputHandler();
    }

    const std::string GlfwInputModule::GetName() const
    {
        return "Glfw Input";
    }

    const std::string GlfwInputModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    const int GlfwInputModule::GetVersion() const
    {
        return 0;
    }
}

Toybox::InputModule* Load()
{
    return new GlfwInput::GlfwInputModule();
}