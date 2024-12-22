#include "GlfwInputModule.h"
#include "GlfwInputHandler.h"
#include <GLFW/glfw3.h>

namespace GlfwInput
{
    Toybox::IInputHandler* GlfwInputModule::Create()
    {
        return new GlfwInputHandler();
    }

    void GlfwInputModule::Destroy(Toybox::IInputHandler* handlerToDestroy)
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

Toybox::Module* Load()
{
    const bool& success = glfwInit();
    TBX_ASSERT(success, "Failed to initialize glfw!");
    return new GlfwInput::GlfwInputModule();
}

void Unload(Toybox::Module* moduleToUnload)
{
    delete moduleToUnload;
}