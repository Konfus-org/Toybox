#include "GLFWInputHandlerFactory.h"
#include "GLFWInputHandler.h"
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    Tbx::IInputHandler* GLFWInputHandlerFactory::Create()
    {
        return new GLFWInputHandler();
    }

    void GLFWInputHandlerFactory::Destroy(Tbx::IInputHandler* handlerToDestroy)
    {
        delete handlerToDestroy;
    }

    std::string GLFWInputHandlerFactory::GetName() const
    {
        return "GLFW Input";
    }

    std::string GLFWInputHandlerFactory::GetAuthor() const
    {
        return "Jeremy Hummel";
    }

    int GLFWInputHandlerFactory::GetVersion() const
    {
        return 0;
    }
}

Tbx::Module* Load()
{
    return new GLFWInput::GLFWInputHandlerFactory();
}

void Unload(Tbx::Module* moduleToUnload)
{
    delete moduleToUnload;
}