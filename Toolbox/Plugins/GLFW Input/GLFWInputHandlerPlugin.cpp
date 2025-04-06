#include "GLFWInputHandlerPlugin.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    void GLFWInputHandlerPlugin::OnLoad()
    {
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

    void GLFWInputHandlerPlugin::OnUnload()
    {
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
    }
    
    void GLFWInputHandlerPlugin::OnSetContextRequestEvent(Tbx::SetInputContextRequest& e)
    {
        SetContext(e.GetContext());
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyDownEvent(Tbx::IsKeyDownRequest& e) const
    {
        auto result = IsKeyDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyUpEvent(Tbx::IsKeyUpRequest& e) const
    {
        auto result = IsKeyUp(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyHeldEvent(Tbx::IsKeyHeldRequest& e) const
    {
        auto result = IsKeyHeld(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsMouseButtonDownEvent(Tbx::IsMouseButtonDownRequest& e) const
    {
        auto result = IsMouseButtonDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsMouseButtonUpEvent(Tbx::IsMouseButtonUpRequest& e) const
    {
        auto result = IsMouseButtonUp(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsMouseButtonHeldEvent(Tbx::IsMouseButtonHeldRequestEvent& e) const
    {
        auto result = IsMouseButtonHeld(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnGetMousePositionEvent(Tbx::GetMousePositionRequest& e) const
    {
        auto result = GetMousePosition();
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonDownEvent(Tbx::IsGamepadButtonDownRequest& e) const
    {
        auto result = IsGamepadButtonDown(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonUpEvent(Tbx::IsGamepadButtonUpRequest& e) const
    {
        auto result = IsGamepadButtonUp(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonHeldEvent(Tbx::IsGamepadButtonHeldRequest& e) const
    {
        auto result = IsGamepadButtonHeld(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(GLFWInput::GLFWInputHandlerPlugin);