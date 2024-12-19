#pragma once
#include "Math/Math.h"
#include "IInputHandler.h"

namespace Toybox
{
    TOYBOX_API class Input
    {
    public:
        static void StartHandling();
        static void StopHandling();

        static bool IsGamepadButtonDown(const int id, const int button);
        static bool IsGamepadButtonUp(const int id, const int button);
        static bool IsGamepadButtonHeld(const int id, const int button);

        static bool IsKeyDown(const int keyCode);
        static bool IsKeyUp(const int keyCode);
        static bool IsKeyHeld(const int keyCode);

        static bool IsMouseButtonDown(const int button);
        static bool IsMouseButtonUp(const int button);
        static bool IsMouseButtonHeld(const int button);
        static Vector2 GetMousePosition();

    private:
        static IInputHandler* _handler;
    };
}
