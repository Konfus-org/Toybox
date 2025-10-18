#pragma once
#include "Tbx/Input/IInputHandler.h"

namespace Tbx
{
    class HeadlessInputHandler final : public IInputHandler
    {
    public:
        HeadlessInputHandler() = default;
        ~HeadlessInputHandler() override = default;

        void Update() override;

        bool IsGamepadButtonDown(int, int) const override;
        bool IsGamepadButtonUp(int, int) const override;
        bool IsGamepadButtonHeld(int, int) const override;
        float GetGamepadAxis(int, int) const override;

        bool IsKeyDown(int) const override;
        bool IsKeyUp(int) const override;
        bool IsKeyHeld(int) const override;

        bool IsMouseButtonDown(int) const override;
        bool IsMouseButtonUp(int) const override;
        bool IsMouseButtonHeld(int) const override;
        Vector2 GetMousePosition() const override;
        Vector2 GetMouseDelta() const override;
    };
}
