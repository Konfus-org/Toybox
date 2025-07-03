#include "Tbx/PCH.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Events/InputEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    UID Input::_windowFocusChangedEventId;

    void Input::Initialize()
    {
        _windowFocusChangedEventId =
            EventCoordinator::Subscribe<WindowFocusedEvent>(TBX_BIND_STATIC_FN(OnWindowFocusChanged));
    }

    void Input::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowFocusedEvent>(_windowFocusChangedEventId);
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        return false;
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        return false;
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        return false;
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        return false;
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        return false;
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        return false;
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        return false;
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        return false;
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        return false;
    }

    Vector2 Input::GetMousePosition()
    {
        return { 0, 0 };
    }

    void Input::OnWindowFocusChanged(const WindowFocusedEvent& e)
    {
    }
}