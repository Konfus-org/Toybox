#include "tbxpch.h"
#include "Input.h"
#include "IInputHandler.h"
#include "Modules/Modules.h"

namespace Toybox
{
    void Input::Init()
    {
        _handler = ((InputModule*)ModuleServer::GetModule("Glfw Input"))->Create();
    }

    void Input::Stop()
    {
        delete _handler;
    }

    bool Input::IsGamepadButtonDown(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonDown(id, inputCode);
    }

    bool Input::IsGamepadButtonUp(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonUp(id, inputCode);
    }

    bool Input::IsGamepadButtonHeld(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonHeld(id, inputCode);
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        return _handler->IsKeyDown(inputCode);
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        return _handler->IsKeyUp(inputCode);
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        return _handler->IsKeyHeld(inputCode);
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        return _handler->IsMouseButtonDown(button);
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        return _handler->IsMouseButtonUp(button);
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        return _handler->IsMouseButtonHeld(button);
    }

    Vector2 Input::GetMousePosition()
    {
        return _handler->GetMousePosition();
    }
}