#include "Tbx/PCH.h"
#include "Tbx/Input/Input.h"
#include "Tbx/App/App.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/Events/InputEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    Uid Input::_windowFocusChangedEventId;
    std::weak_ptr<IInputHandlerPlugin> Input::_inputHandler = {};

    void Input::Initialize()
    {
        _windowFocusChangedEventId = EventCoordinator::Subscribe<WindowFocusedEvent>(TBX_BIND_STATIC_FN(OnWindowFocusChanged));
        _inputHandler = PluginServer::Get<IInputHandlerPlugin>();
        TBX_VALIDATE_WEAK_PTR(_inputHandler, "Input handler plugin not found!");
    }

    void Input::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowFocusedEvent>(_windowFocusChangedEventId);
        _inputHandler.reset();
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }

        return _inputHandler.lock()->IsGamepadButtonDown(id, button);
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsGamepadButtonUp(id, button);
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsGamepadButtonHeld(id, button);
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsKeyDown(inputCode);
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsKeyUp(inputCode);
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsKeyHeld(inputCode);
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsMouseButtonDown(button);
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        return _inputHandler.lock()->IsMouseButtonUp(button);
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->IsMouseButtonHeld(button);
    }

    Vector2 Input::GetMousePosition()
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return false;
        }
        return _inputHandler.lock()->GetMousePosition();
    }

    void Input::OnWindowFocusChanged(const WindowFocusedEvent& e)
    {
        if (_inputHandler.expired() || !_inputHandler.lock())
        {
            return;
        }
        _inputHandler.lock()->SetContext(App::GetInstance()->GetWindow(e.GetWindowId()));
    }
}