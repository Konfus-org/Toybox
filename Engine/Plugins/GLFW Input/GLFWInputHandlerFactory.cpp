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

    void GLFWInputHandlerFactory::OnLoad()
    {
    }

    void GLFWInputHandlerFactory::OnUnload()
    {
    }
}

TBX_REGISTER_PLUGIN(GLFWInput::GLFWInputHandlerFactory);