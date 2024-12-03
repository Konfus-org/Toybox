#include "GlfwInputModule.h"
#include "GlfwInputHandler.h"

namespace GlfwInput
{
    Toybox::IInputHandler* GlfwInputModule::CreateInputHandler(void* mainNativeWindow)
    {
        return new GlfwInputHandler(mainNativeWindow);
    }

    void GlfwInputModule::DestroyInputHandler(Toybox::IInputHandler* handlerToDestroy)
    {
        delete handlerToDestroy;
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

Toybox::InputModule* _module = nullptr;
Toybox::InputModule* Load()
{
    if (_module == nullptr) _module = new GlfwInput::GlfwInputModule();
    return _module;
}

void Unload()
{
    delete _module;
}