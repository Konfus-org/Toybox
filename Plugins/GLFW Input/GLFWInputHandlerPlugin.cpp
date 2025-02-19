#include "GLFWInputHandlerPlugin.h"
#include "GLFWInputHandler.h"
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    Tbx::IInputHandler* GLFWInputHandlerPlugin::Provide()
    {
        return new GLFWInputHandler();
    }

    void GLFWInputHandlerPlugin::Destroy(Tbx::IInputHandler* handlerToDestroy)
    {
        delete handlerToDestroy;
    }

    void GLFWInputHandlerPlugin::OnLoad()
    {
        // Input handler does nothing on load... glfw initialization is done from the windowing plugin
    }

    void GLFWInputHandlerPlugin::OnUnload()
    {
        // Input handler does nothing on unlow... glfw termination is done from the windowing plugin
    }
}

TBX_REGISTER_PLUGIN(GLFWInput::GLFWInputHandlerPlugin);