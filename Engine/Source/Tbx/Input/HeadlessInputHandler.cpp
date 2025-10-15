#include "Tbx/PCH.h"
#include "Tbx/Input/HeadlessInputHandler.h"

namespace Tbx
{
    void HeadlessInputHandler::UpdateInputState()
    {
    }

    bool HeadlessInputHandler::IsGamepadButtonDown(int, int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsGamepadButtonUp(int, int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsGamepadButtonHeld(int, int) const
    {
        return false;
    }

    float HeadlessInputHandler::GetGamepadAxis(int, int) const
    {
        return 0.0f;
    }

    bool HeadlessInputHandler::IsKeyDown(int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsKeyUp(int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsKeyHeld(int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsMouseButtonDown(int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsMouseButtonUp(int) const
    {
        return false;
    }

    bool HeadlessInputHandler::IsMouseButtonHeld(int) const
    {
        return false;
    }

    Vector2 HeadlessInputHandler::GetMousePosition() const
    {
        return {};
    }

    Vector2 HeadlessInputHandler::GetMouseDelta() const
    {
        return {};
    }
}
