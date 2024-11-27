#include "tbxpch.h"
#include "Input.h"
#include "IInputHandler.h"
#include "Modules/Modules.h"

namespace Toybox::Input
{
    static IInputHandler* _handler = Modules::ModuleServer::GetInstance()->GetModule<Modules::InputModule>()->Create();

    bool IsGamepadButtonDown(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonDown(id, inputCode);
    }

    bool IsGamepadButtonUp(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonUp(id, inputCode);
    }

    bool IsGamepadButtonHeld(const int id, const int inputCode)
    {
        return _handler->IsGamepadButtonHeld(id, inputCode);
    }

    bool IsKeyDown(const int inputCode)
    {
        return _handler->IsKeyDown(inputCode);
    }

    bool IsKeyUp(const int inputCode)
    {
        return _handler->IsKeyUp(inputCode);
    }

    bool IsKeyHeld(const int inputCode)
    {
        return _handler->IsKeyHeld(inputCode);
    }

    bool IsMouseButtonDown(const int button)
    {
        return _handler->IsMouseButtonDown(button);
    }

    bool IsMouseButtonUp(const int button)
    {
        return _handler->IsMouseButtonUp(button);
    }

    bool IsMouseButtonHeld(const int button)
    {
        return _handler->IsMouseButtonHeld(button);
    }

    Math::Vector2 GetMousePosition()
    {
        return _handler->GetMousePosition();
    }
}