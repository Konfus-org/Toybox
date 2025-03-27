#include "App/Input/Input.h"
#include "App/Windowing/IWindow.h"
#include "App/Plugins/PluginServer.h"
#include <Core/Debug/DebugAPI.h>

namespace Tbx
{
    std::shared_ptr<IInputHandler> Input::_handler;
    std::weak_ptr<IWindow> Input::_context;

    void Input::Initialize()
    {
        _handler = PluginServer::GetPlugin<IInputHandler>();
        TBX_VALIDATE_PTR(_handler, "Failed to load input plugin!");
    }

    void Input::Stop()
    {
        _handler.reset();
        _context.reset();
    }

    void Input::SetContext(const std::weak_ptr<IWindow>& context)
    {
        _handler->SetContext(context);
        _context = context;
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        return _handler->IsGamepadButtonDown(id, button);
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        return _handler->IsGamepadButtonUp(id, button);
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        return _handler->IsGamepadButtonHeld(id, button);
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
        if (_handler != nullptr)
        {
            TBX_ERROR("Handler is null! Cannot get mouse position.");
            return Vector2();
        }

        return _handler->GetMousePosition();
    }
}