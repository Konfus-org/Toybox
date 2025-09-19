#include "Tbx/PCH.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    std::shared_ptr<IInputHandler> Input::_inputHandler = {};

    // TODO: Input CANNOT be static, it should be owned by the app.
    // We can enable static like usage or make a better way.

    void Input::Initialize(const std::shared_ptr<IInputHandler>& inputHandler)
    {
        TBX_ASSERT(inputHandler, "Input handler was null!");

        if (_inputHandler)
        {
            TBX_ASSERT(false, "Input was already initialized!");
            return;
        }

        _inputHandler = inputHandler;
    }

    void Input::Shutdown()
    {
        if (!_inputHandler)
        {
            TBX_ASSERT(false, "Input was never initialized!");
            return;
        }

        _inputHandler.reset();
    }

    bool Input::EnsureHandler()
    {
        TBX_ASSERT(_inputHandler, "Input has not been initialized yet! Did you call Input::Initialize?");
        return static_cast<bool>(_inputHandler);
    }

    void Input::Update()
    {
        if (!EnsureHandler())
        {
            return;
        }

        _inputHandler->Update();
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
