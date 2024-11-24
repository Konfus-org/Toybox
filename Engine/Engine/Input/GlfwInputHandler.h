#pragma once
#include "IInputHandler.h"

namespace Toybox::Input
{
    class GlfwInputHandler : public IInputHandler
    {
    public:
        // Controller input
        bool IsGamepadButtonDown(const int inputCode) override;
        bool IsGamepadButtonUp(const int inputCode) override;
        bool IsGamepadButtonHeld(const int inputCode) override;

        // Mouse and keyboard input
        bool IsKeyDown(const int inputCode) override;
        bool IsKeyUp(const int inputCode) override;
        bool IsKeyHeld(const int inputCode) override;
    };
}

