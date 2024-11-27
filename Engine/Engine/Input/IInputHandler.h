#pragma once
#include "Math/Math.h"

namespace Toybox::Input
{
    class IInputHandler
    {
    public:
        virtual bool IsGamepadButtonDown(const int id, const int button) = 0;
        virtual bool IsGamepadButtonUp(const int id, const int button) = 0;
        virtual bool IsGamepadButtonHeld(const int id, const int button) = 0;

        virtual bool IsKeyDown(const int keyCode) = 0;
        virtual bool IsKeyUp(const int keyCode) = 0;
        virtual bool IsKeyHeld(const int keyCode) = 0;

        virtual bool IsMouseButtonDown(const int button) = 0;
        virtual bool IsMouseButtonUp(const int button) = 0;
        virtual bool IsMouseButtonHeld(const int button) = 0;
        virtual Math::Vector2 GetMousePosition() = 0;
    };
}