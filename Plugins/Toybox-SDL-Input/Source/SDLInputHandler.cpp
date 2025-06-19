#include "SDLInputHandler.h"

namespace SDLInput
{
    void SDLInputHandler::OnLoad()
    {
    }

    void SDLInputHandler::OnUnload()
    {
    }

    void SDLInputHandler::SetContext(const std::shared_ptr<Tbx::IWindow>& windowToListenTo)
    {
    }

    bool SDLInputHandler::IsGamepadButtonDown(const int gamepadId, const int button) const
    {
        return false;
    }

    bool SDLInputHandler::IsGamepadButtonUp(const int gamepadId, const int button) const
    {
        return false;
    }

    bool SDLInputHandler::IsGamepadButtonHeld(const int gamepadId, const int button) const
    {
        return false;
    }

    bool SDLInputHandler::IsKeyDown(const int keyCode) const
    {
        return false;
    }

    bool SDLInputHandler::IsKeyUp(const int keyCode) const
    {
        return false;
    }

    bool SDLInputHandler::IsKeyHeld(const int keyCode) const
    {
        return false;
    }

    bool SDLInputHandler::IsMouseButtonDown(const int button) const
    {
        return false;
    }

    bool SDLInputHandler::IsMouseButtonUp(const int button) const
    {
        return false;
    }

    bool SDLInputHandler::IsMouseButtonHeld(const int button) const
    {
        return false;
    }

    Tbx::Vector2 SDLInputHandler::GetMousePosition() const
    {
        return Tbx::Vector2();
    }
}
