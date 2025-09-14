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
        _inputHandler = inputHandler;
    }

    void Input::Shutdown()
    {
        _inputHandler.reset();
    }

    void Input::Update()
    {
        _inputHandler->Update();
    }

    bool Input::IsGamepadButtonDown(const int playerIndex, const int button)
    {
        return _inputHandler->IsGamepadButtonDown(playerIndex, button);
    }

    bool Input::IsGamepadButtonUp(const int playerIndex, const int button)
    {
        return _inputHandler->IsGamepadButtonUp(playerIndex, button);
    }

    bool Input::IsGamepadButtonHeld(const int playerIndex, const int button)
    {
        return _inputHandler->IsGamepadButtonHeld(playerIndex, button);
    }

    float Input::GetGamepadAxis(const int playerIndex, const int axis)
    {
        return _inputHandler->GetGamepadAxis(playerIndex, axis);
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

    Vector2 Input::GetMouseDelta()
    {
        return _inputHandler->GetMouseDelta();
    }
}