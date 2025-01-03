#include "GLFWInputModule.h"
#include "GLFWInputHandler.h"
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    Tbx::IInputHandler* GLFWInputModule::Create()
    {
        return new GLFWInputHandler();
    }

    void GLFWInputModule::Destroy(Tbx::IInputHandler* handlerToDestroy)
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

Tbx::Module* Load()
{
    return new GLFWInput::GLFWInputModule();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}