#include "GLFWInputModule.h"
#include "GLFWInputHandler.h"
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    Toybox::IInputHandler* GLFWInputModule::Create()
    {
        return new GLFWInputHandler();
    }

    void GLFWInputModule::Destroy(Toybox::IInputHandler* handlerToDestroy)
    {
        delete handlerToDestroy;
    }

    std::string GLFWInputModule::GetName() const
    {
        return "GLFW Input";
    }

    std::string GLFWInputModule::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GLFWInputModule::GetVersion() const
    {
        return 0;
    }
}

Toybox::Module* Load()
{
    return new GLFWInput::GLFWInputModule();
}

void Unload(Toybox::Module* moduleToUnload)
{
    delete moduleToUnload;
}