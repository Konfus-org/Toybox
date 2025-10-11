#include "Tbx/PCH.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Debug/Tracers.h"

namespace Tbx
{
    Ref<IInputHandler> Input::_inputHandler = {};

    void Input::SetHandler(const Ref<IInputHandler>& inputHandler)
    {
        TBX_ASSERT(inputHandler, "Input: handler was null!");
        _inputHandler = inputHandler;
    }

    void Input::ClearHandler()
    {
        _inputHandler.reset();
    }

    bool Input::EnsureHandler()
    {
        if (!_inputHandler)
        {
            TBX_TRACE_WARNING("Input: no handler has been set!");
        }
        return static_cast<bool>(_inputHandler);
    }

    void Input::Update()
    {
        if (!EnsureHandler())
        {
            return;
        }

        _inputHandler->RefreshInputStates();
    }

    bool Input::IsGamepadButtonDown(const int playerIndex, const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsGamepadButtonDown(playerIndex, button);
    }

    bool Input::IsGamepadButtonUp(const int playerIndex, const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsGamepadButtonUp(playerIndex, button);
    }

    bool Input::IsGamepadButtonHeld(const int playerIndex, const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsGamepadButtonHeld(playerIndex, button);
    }

    float Input::GetGamepadAxis(const int playerIndex, const int axis)
    {
        if (!EnsureHandler())
        {
            return 0.0f;
        }

        return _inputHandler->GetGamepadAxis(playerIndex, axis);
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsKeyDown(inputCode);
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsKeyUp(inputCode);
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsKeyHeld(inputCode);
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsMouseButtonDown(button);
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsMouseButtonUp(button);
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        if (!EnsureHandler())
        {
            return false;
        }

        return _inputHandler->IsMouseButtonHeld(button);
    }

    Vector2 Input::GetMousePosition()
    {
        if (!EnsureHandler())
        {
            return {};
        }

        return _inputHandler->GetMousePosition();
    }

    Vector2 Input::GetMouseDelta()
    {
        if (!EnsureHandler())
        {
            return {};
        }

        return _inputHandler->GetMouseDelta();
    }
}
