#include "GLFWWindowingPlugin.h"
#include "GLFWWindowFactory.h"
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    Tbx::IWindowFactory* GLFWWindowing::GLFWWindowingPlugin::Provide()
    {
        return _windowFactory.get();
    }

    void GLFWWindowingPlugin::Destroy(Tbx::IWindowFactory* toDestroy)
    {
        delete toDestroy;
    }

    void GLFWWindowingPlugin::OnLoad()
    {
        _windowFactory = std::make_shared<GLFWWindowFactory>();
        _openNewWindowRequestEventId = Tbx::EventDispatcher::Subscribe<Tbx::OpenNewWindowRequestEvent>(TBX_BIND_CALLBACK(OnOpenNewWindow));

        const auto& status = glfwInit();
        TBX_ASSERT(status, "Failed to initialize GLFW!");
    }

    void GLFWWindowingPlugin::OnUnload()
    {
        glfwTerminate();
    }

    void GLFWWindowingPlugin::OnOpenNewWindow(Tbx::OpenNewWindowRequestEvent& e)
    {
        auto newWindow = _windowFactory->Create(e.GetName(), e.GetSize());
        newWindow->Open(e.GetMode());
        e.SetResult(newWindow);
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(GLFWWindowing::GLFWWindowingPlugin);