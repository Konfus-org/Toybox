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

    ////bool Input::IsGamepadButtonDown(const int id, const int button)
    ////{
    ////    return _handler->IsGamepadButtonDown(id, button);
    ////}

    ////bool Input::IsGamepadButtonUp(const int id, const int button)
    ////{
    ////    return _handler->IsGamepadButtonUp(id, button);
    ////}

    ////bool Input::IsGamepadButtonHeld(const int id, const int button)
    ////{
    ////    return _handler->IsGamepadButtonHeld(id, button);
    ////}

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

    ////bool Input::IsKeyHeld(const int inputCode)
    ////{
    ////    return _handler->IsKeyHeld(inputCode);
    ////}

    ////bool Input::IsMouseButtonDown(const int button)
    ////{
    ////    return _handler->IsMouseButtonDown(button);
    ////}

    ////bool Input::IsMouseButtonUp(const int button)
    ////{
    ////    return _handler->IsMouseButtonUp(button);
    ////}

    ////bool Input::IsMouseButtonHeld(const int button)
    ////{
    ////    return _handler->IsMouseButtonHeld(button);
    ////}

    ////Vector2 Input::GetMousePosition()
    ////{
    ////    if (_handler != nullptr)
    ////    {
    ////        TBX_ERROR("Handler is null! Cannot get mouse position.");
    ////        return Vector2();
    ////    }

    ////    return _handler->GetMousePosition();
    ////}

    void Input::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        SetInputContextRequestEvent request(WindowManager::GetWindow(e.GetWindowId()));
        EventDispatcher::Dispatch(request);
    }
}