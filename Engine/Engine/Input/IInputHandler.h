#pragma once

namespace Toybox::Input
{
    class IInputHandler
    {
    public:
        // Controller
        virtual bool IsGamepadButtonDown(const int inputCode) = 0;
        virtual bool IsGamepadButtonUp(const int inputCode) = 0;
        virtual bool IsGamepadButtonHeld(const int inputCode) = 0;

        // Mouse and keyboard
        virtual bool IsKeyDown(const int inputCode) = 0;
        virtual bool IsKeyUp(const int inputCode) = 0;
        virtual bool IsKeyHeld(const int inputCode) = 0;
    };
}