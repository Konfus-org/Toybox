#pragma once
#include "Math/Math.h"

namespace Toybox::Input
{
    static bool IsGamepadButtonDown(const int id, const int button);
    static bool IsGamepadButtonUp(const int id, const int button);
    static bool IsGamepadButtonHeld(const int id, const int button);

    static bool IsKeyDown(const int keyCode);
    static bool IsKeyUp(const int keyCode);
    static bool IsKeyHeld(const int keyCode);

    static bool IsMouseButtonDown(const int button);
    static bool IsMouseButtonUp(const int button);
    static bool IsMouseButtonHeld(const int button);
    static Math::Vector2 GetMousePosition();
}
