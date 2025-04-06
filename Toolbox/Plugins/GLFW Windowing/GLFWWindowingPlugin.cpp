#include "GLFWWindowingPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/App/Events/WindowEvents.h>
#include <Tbx/Core/Debug/DebugAPI.h>
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    void GLFWWindowingPlugin::OnLoad()
    {
        _windowFactory = std::make_shared<GLFWWindowFactory>();
        _openNewWindowRequestEventId = Tbx::EventCoordinator::Subscribe<Tbx::OpenNewWindowRequest>(TBX_BIND_FN(OnOpenNewWindow));

        const auto& status = glfwInit();
        TBX_ASSERT(status, "Failed to initialize GLFW!");
    }

    void GLFWWindowingPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::OpenNewWindowRequest>(_openNewWindowRequestEventId);

        _windowFactory.reset();
        glfwTerminate();
    }

    void GLFWWindowingPlugin::OnOpenNewWindow(Tbx::OpenNewWindowRequest& e)
    {
        auto newWindow = _windowFactory->Create(e.GetName(), e.GetSize());
        newWindow->Open(e.GetMode());
        newWindow->Focus();
        e.SetResult(newWindow);
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(GLFWWindowing::GLFWWindowingPlugin);