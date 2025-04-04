#include "GLFWInputHandlerPlugin.h"
#include <Tbx/Core/Events/EventDispatcher.h>
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    void GLFWInputHandlerPlugin::OnLoad()
    {
        _onSetContextRequestEventUID = Tbx::EventDispatcher::Subscribe<Tbx::SetInputContextRequestEvent>(TBX_BIND_CALLBACK(OnSetContextRequestEvent));

        _onIsKeyDownEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsKeyDownRequestEvent>(TBX_BIND_CALLBACK(OnIsKeyDownEvent));
        _onIsKeyUpEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsKeyUpRequestEvent>(TBX_BIND_CALLBACK(OnIsKeyUpEvent));
        _onIsKeyHeldEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsKeyHeldRequestEvent>(TBX_BIND_CALLBACK(OnIsKeyHeldEvent));

        _onGetMousePositionEventUID = Tbx::EventDispatcher::Subscribe<Tbx::GetMousePositionRequestEvent>(TBX_BIND_CALLBACK(OnGetMousePositionEvent));

        _onIsMouseButtonDownEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsMouseButtonDownRequestEvent>(TBX_BIND_CALLBACK(OnIsMouseButtonDownEvent));
        _onIsMouseButtonUpEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsMouseButtonUpRequestEvent>(TBX_BIND_CALLBACK(OnIsMouseButtonUpEvent));
        _onIsMouseButtonHeldEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsMouseButtonHeldRequestEvent>(TBX_BIND_CALLBACK(OnIsMouseButtonHeldEvent));

        _onIsGamepadButtonDownEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsGamepadButtonDownRequestEvent>(TBX_BIND_CALLBACK(OnIsGamepadButtonDownEvent));
        _onIsGamepadButtonUpEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsGamepadButtonUpRequestEvent>(TBX_BIND_CALLBACK(OnIsGamepadButtonUpEvent));
        _onIsGamepadButtonHeldEventUID = Tbx::EventDispatcher::Subscribe<Tbx::IsGamepadButtonHeldRequestEvent>(TBX_BIND_CALLBACK(OnIsGamepadButtonHeldEvent));
    }

    void GLFWInputHandlerPlugin::OnUnload()
    {
        Tbx::EventDispatcher::Unsubscribe(_onSetContextRequestEventUID);

        Tbx::EventDispatcher::Unsubscribe(_onIsKeyDownEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsKeyUpEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsKeyHeldEventUID);

        Tbx::EventDispatcher::Unsubscribe(_onGetMousePositionEventUID);

        Tbx::EventDispatcher::Unsubscribe(_onIsMouseButtonDownEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsMouseButtonUpEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsMouseButtonHeldEventUID);

        Tbx::EventDispatcher::Unsubscribe(_onIsGamepadButtonDownEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsGamepadButtonUpEventUID);
        Tbx::EventDispatcher::Unsubscribe(_onIsGamepadButtonHeldEventUID);
    }
    
    void GLFWInputHandlerPlugin::OnSetContextRequestEvent(Tbx::SetInputContextRequestEvent& e)
    {
        SetContext(e.GetContext());
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyDownEvent(Tbx::IsKeyDownRequestEvent& e) const
    {
        auto result = IsKeyDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyUpEvent(Tbx::IsKeyUpRequestEvent& e) const
    {
        auto result = IsKeyUp(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsKeyHeldEvent(Tbx::IsKeyHeldRequestEvent& e) const
    {
        auto result = IsKeyHeld(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsMouseButtonDownEvent(Tbx::IsMouseButtonDownRequestEvent& e) const
    {
        auto result = IsMouseButtonDown(e.GetKeyCode());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsMouseButtonUpEvent(Tbx::IsMouseButtonUpRequestEvent& e) const
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

    void GLFWInputHandlerPlugin::OnGetMousePositionEvent(Tbx::GetMousePositionRequestEvent& e) const
    {
        auto result = GetMousePosition();
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonDownEvent(Tbx::IsGamepadButtonDownRequestEvent& e) const
    {
        auto result = IsGamepadButtonDown(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonUpEvent(Tbx::IsGamepadButtonUpRequestEvent& e) const
    {
        auto result = IsGamepadButtonUp(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }

    void GLFWInputHandlerPlugin::OnIsGamepadButtonHeldEvent(Tbx::IsGamepadButtonHeldRequestEvent& e) const
    {
        auto result = IsGamepadButtonHeld(e.GetKeyCode(), e.GetGamepadId());
        e.SetResult(result);
        e.IsHandled = true;
    }
}

TBX_REGISTER_PLUGIN(GLFWInput::GLFWInputHandlerPlugin);