#include "Tbx/App/PCH.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Windowing/IWindow.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/InputEvents.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    UID Input::_windowFocusChangedEventId;

    void Input::Initialize()
    {
        _windowFocusChangedEventId = 
            EventCoordinator::Subscribe<WindowFocusChangedEvent>(TBX_BIND_STATIC_FN(OnWindowFocusChanged));
    }

    void Input::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowFocusChangedEvent>(_windowFocusChangedEventId);
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        IsGamepadButtonDownRequest request(id, button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        IsGamepadButtonUpRequest request(id, button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        IsGamepadButtonHeldRequest request(id, button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        IsKeyDownRequest request(inputCode);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        IsKeyUpRequest request(inputCode);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        IsKeyHeldRequest request(inputCode);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        IsMouseButtonDownRequest request(button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        IsMouseButtonUpRequest request(button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        IsMouseButtonHeldRequestEvent request(button);
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    Vector2 Input::GetMousePosition()
    {
        GetMousePositionRequest request;
        EventCoordinator::Send(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    void Input::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        SetInputContextRequest request(WindowManager::GetWindow(e.GetWindowId()));
        EventCoordinator::Send(request);
    }
}