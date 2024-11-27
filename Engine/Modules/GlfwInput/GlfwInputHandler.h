#pragma once
#include <Toybox.h>

namespace GlfwInput
{
    class GlfwInputHandler : public Toybox::Input::IInputHandler
    {
    public:
        bool IsGamepadButtonDown(const int id, const int button) override;
        bool IsGamepadButtonUp(const int id, const int button) override;
        bool IsGamepadButtonHeld(const int id, const int button) override;

        bool IsKeyDown(const int keyCode) override;
        bool IsKeyUp(const int keyCode) override;
        bool IsKeyHeld(const int keyCode) override;

        bool IsMouseButtonDown(const int button) override;
        bool IsMouseButtonUp(const int button) override;
        bool IsMouseButtonHeld(const int button) override;
        Toybox::Math::Vector2 GetMousePosition() override;
    };
}

