#include "Tbx/App/PCH.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Windowing/IWindow.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/InputEvents.h"
#include <Tbx/Core/Events/EventDispatcher.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    UID Input::_windowFocusChangedEventId;

    void Input::Initialize()
    {
        _windowFocusChangedEventId = 
            EventDispatcher::Subscribe<WindowFocusChangedEvent>(TBX_BIND_STATIC_CALLBACK(OnWindowFocusChanged));
    }

    void Input::Shutdown()
    {
        EventDispatcher::Unsubscribe(_windowFocusChangedEventId);
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        IsGamepadButtonDownRequestEvent request(id, button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        IsGamepadButtonUpRequestEvent request(id, button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        IsGamepadButtonHeldRequestEvent request(id, button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        IsKeyDownRequestEvent request(inputCode);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        IsKeyUpRequestEvent request(inputCode);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        IsKeyHeldRequestEvent request(inputCode);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        IsMouseButtonDownRequestEvent request(button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        IsMouseButtonUpRequestEvent request(button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        IsMouseButtonHeldRequestEvent request(button);
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    Vector2 Input::GetMousePosition()
    {
        GetMousePositionRequestEvent request;
        EventDispatcher::Dispatch(request);
        TBX_ASSERT(request.IsHandled, "Input code not handled! Do we have a handler created and listening?");

        return request.GetResult();
    }

    void Input::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        SetInputContextRequestEvent request(WindowManager::GetWindow(e.GetWindowId()));
        EventDispatcher::Dispatch(request);
    }
}