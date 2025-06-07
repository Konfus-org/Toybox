#include "GLFWWinputPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <GLFW/glfw3.h>
#include <Tbx/Core/Rendering/IRenderer.h>

namespace GLFWPlugin
{
    void GLFWWinputPlugin::OnLoad()
    {
        _openNewWindowRequestEventId = Tbx::EventCoordinator::Subscribe<Tbx::OpenNewWindowRequest>(TBX_BIND_FN(OnOpenNewWindow));

        const auto& status = glfwInit();
        TBX_ASSERT(status, "Failed to initialize GLFW!");

        _onSetContextRequestEventUID = Tbx::EventCoordinator::Subscribe<Tbx::SetInputContextRequest>(TBX_BIND_FN(OnSetContextRequestEvent));

        _onIsKeyDownEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsKeyDownRequest>(TBX_BIND_FN(OnIsKeyDownEvent));
        _onIsKeyUpEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsKeyUpRequest>(TBX_BIND_FN(OnIsKeyUpEvent));
        _onIsKeyHeldEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsKeyHeldRequest>(TBX_BIND_FN(OnIsKeyHeldEvent));

        _onGetMousePositionEventUID = Tbx::EventCoordinator::Subscribe<Tbx::GetMousePositionRequest>(TBX_BIND_FN(OnGetMousePositionEvent));

        _onIsMouseButtonDownEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsMouseButtonDownRequest>(TBX_BIND_FN(OnIsMouseButtonDownEvent));
        _onIsMouseButtonUpEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsMouseButtonUpRequest>(TBX_BIND_FN(OnIsMouseButtonUpEvent));
        _onIsMouseButtonHeldEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsMouseButtonHeldRequestEvent>(TBX_BIND_FN(OnIsMouseButtonHeldEvent));

        _onIsGamepadButtonDownEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsGamepadButtonDownRequest>(TBX_BIND_FN(OnIsGamepadButtonDownEvent));
        _onIsGamepadButtonUpEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsGamepadButtonUpRequest>(TBX_BIND_FN(OnIsGamepadButtonUpEvent));
        _onIsGamepadButtonHeldEventUID = Tbx::EventCoordinator::Subscribe<Tbx::IsGamepadButtonHeldRequest>(TBX_BIND_FN(OnIsGamepadButtonHeldEvent));
    }

    void GLFWWinputPlugin::OnUnload()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::OpenNewWindowRequest>(_openNewWindowRequestEventId);

        Tbx::EventCoordinator::Unsubscribe<Tbx::SetInputContextRequest>(_onSetContextRequestEventUID);

        Tbx::EventCoordinator::Unsubscribe<Tbx::IsKeyDownRequest>(_onIsKeyDownEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsKeyUpRequest>(_onIsKeyUpEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsKeyHeldRequest>(_onIsKeyHeldEventUID);

        Tbx::EventCoordinator::Unsubscribe<Tbx::GetMousePositionRequest>(_onGetMousePositionEventUID);

        Tbx::EventCoordinator::Unsubscribe<Tbx::IsMouseButtonDownRequest>(_onIsMouseButtonDownEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsMouseButtonUpRequest>(_onIsMouseButtonUpEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsMouseButtonHeldRequestEvent>(_onIsMouseButtonHeldEventUID);

        Tbx::EventCoordinator::Unsubscribe<Tbx::IsGamepadButtonDownRequest>(_onIsGamepadButtonDownEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsGamepadButtonUpRequest>(_onIsGamepadButtonUpEventUID);
        Tbx::EventCoordinator::Unsubscribe<Tbx::IsGamepadButtonHeldRequest>(_onIsGamepadButtonHeldEventUID);

        glfwTerminate();
    }

    void GLFWWinputPlugin::OnOpenNewWindow(Tbx::OpenNewWindowRequest& e)
    {
        auto renderFactory = Tbx::PluginServer::GetPlugin<Tbx::IRendererFactory>();
        if (renderFactory == nullptr)
        {
            TBX_ASSERT(false, "No render factory plugin found! Cannot create a window because a window required a renderer...");
            return;
        }

        auto newWindow = Create(e.GetName(), e.GetSize());
        newWindow->SetRenderer(renderFactory->Create());
        newWindow->Open(e.GetMode());
        newWindow->Focus();

        e.SetResult(newWindow);
        e.IsHandled = true;
    }

    void OnGlfwError(int error, const char* description)
    {
        TBX_ERROR("GLFW Error ({}): {}", error, description);
    }
    
    void GLFWWinputPlugin::OnSetContextRequestEvent(Tbx::SetInputContextRequest& e)
    {
        SetContext(e.GetContext());
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsKeyDownEvent(Tbx::IsKeyDownRequest& e) const
    {
        auto result = IsKeyDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsKeyUpEvent(Tbx::IsKeyUpRequest& e) const
    {
        auto result = IsKeyUp(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsKeyHeldEvent(Tbx::IsKeyHeldRequest& e) const
    {
        auto result = IsKeyHeld(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsMouseButtonDownEvent(Tbx::IsMouseButtonDownRequest& e) const
    {
        auto result = IsMouseButtonDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsMouseButtonUpEvent(Tbx::IsMouseButtonUpRequest& e) const
    {
        auto result = IsMouseButtonUp(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsMouseButtonHeldEvent(Tbx::IsMouseButtonHeldRequestEvent& e) const
    {
        auto result = IsMouseButtonHeld(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnGetMousePositionEvent(Tbx::GetMousePositionRequest& e) const
    {
        auto result = GetMousePosition();
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsGamepadButtonDownEvent(Tbx::IsGamepadButtonDownRequest& e) const
    {
        auto result = IsGamepadButtonDown(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsGamepadButtonUpEvent(Tbx::IsGamepadButtonUpRequest& e) const
    {
        auto result = IsGamepadButtonUp(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWWinputPlugin::OnIsGamepadButtonHeldEvent(Tbx::IsGamepadButtonHeldRequest& e) const
    {
        auto result = IsGamepadButtonHeld(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }
}