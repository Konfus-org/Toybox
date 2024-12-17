#include "GlfwInputModule.h"
#include "GlfwInputHandler.h"

namespace GlfwInput
{
    Toybox::IInputHandler* GlfwInputModule::CreateInputHandler(std::any mainNativeWindow)
    {
        return new GlfwInputHandler(mainNativeWindow);
    }

    void GlfwInputModule::DestroyInputHandler(Toybox::IInputHandler* handlerToDestroy)
    {
        delete handlerToDestroy;
    }

    std::string GlfwInputModule::GetName() const
    {
        return "Glfw Input";
    }

    std::string GlfwInputModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GlfwInputModule::GetVersion() const
    {
        return 0;
    }
}