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
    std::shared_ptr<IInputHandlerPlugin> Input::_inputHandler = nullptr;

    void Input::Initialize()
    {
        _windowFocusChangedEventId = EventCoordinator::Subscribe<WindowFocusedEvent>(TBX_BIND_STATIC_FN(OnWindowFocusChanged));
        _inputHandler = PluginServer::Get<IInputHandlerPlugin>();
    }

    void Input::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowFocusedEvent>(_windowFocusChangedEventId);
        _inputHandler.reset();
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        return _inputHandler->IsGamepadButtonDown(id, button);
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        return _inputHandler->IsGamepadButtonUp(id, button);
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        return _inputHandler->IsGamepadButtonHeld(id, button);
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        return _inputHandler->IsKeyDown(inputCode);
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        return _inputHandler->IsKeyUp(inputCode);
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        return _inputHandler->IsKeyHeld(inputCode);
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        return _inputHandler->IsMouseButtonDown(button);
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        return _inputHandler->IsMouseButtonUp(button);
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        return _inputHandler->IsMouseButtonHeld(button);
    }

    Vector2 Input::GetMousePosition()
    {
        return _inputHandler->GetMousePosition();
    }

    void Input::OnWindowFocusChanged(const WindowFocusedEvent& e)
    {
        _inputHandler->SetContext(App::GetInstance()->GetWindow(e.GetWindowId()));
    }
}