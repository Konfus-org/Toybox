#include "TbxPCH.h"
#include "Input.h"
#include "Windowing/IWindow.h"
#include "Plugins/PluginServer.h"
#include "Debug/DebugAPI.h"
#include "Util/SmartPointerUtils.h"

#define TBX_VALIDATE_INPUT(error_msg, ...)  if (_handler == nullptr) { TBX_ERROR(error_msg, __VA_ARGS__); return false; }

namespace Tbx
{
    std::shared_ptr<IInputHandler> Input::_handler;
    std::weak_ptr<IWindow> Input::_context;

    void Input::Initialize()
    {
        _handler = PluginServer::GetPlugin<IInputHandler>();
    }

    void Input::Stop()
    {
        _handler.reset();
        _context.reset();
    }

    void Input::SetContext(const std::weak_ptr<IWindow>& context)
    {
        if (!IsWeakPointerValid(_context) || context.lock() != _context.lock())
        {
            _handler->SetContext(context);
            _context = context;
        }
    }

    bool Input::IsGamepadButtonDown(const int id, const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if gamepad button {0} is down.", button);
        return _handler->IsGamepadButtonDown(id, button);
    }

    bool Input::IsGamepadButtonUp(const int id, const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if gamepad button {0} is up.", button);
        return _handler->IsGamepadButtonUp(id, button);
    }

    bool Input::IsGamepadButtonHeld(const int id, const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if gamepad button {0} is held.", button);
        return _handler->IsGamepadButtonHeld(id, button);
    }

    bool Input::IsKeyDown(const int inputCode)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if key {0} is down.", inputCode);
        return _handler->IsKeyDown(inputCode);
    }

    bool Input::IsKeyUp(const int inputCode)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if key {0} is up.", inputCode);
        return _handler->IsKeyUp(inputCode);
    }

    bool Input::IsKeyHeld(const int inputCode)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check if key {0} is held.", inputCode);
        return _handler->IsKeyHeld(inputCode);
    }

    bool Input::IsMouseButtonDown(const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check mouse button {0} down.", button);
        return _handler->IsMouseButtonDown(button);
    }

    bool Input::IsMouseButtonUp(const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check mouse button {0} down.", button);
        return _handler->IsMouseButtonUp(button);
    }

    bool Input::IsMouseButtonHeld(const int button)
    {
        TBX_VALIDATE_INPUT("Handler is null! Cannot check mouse button {0} down.", button);
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