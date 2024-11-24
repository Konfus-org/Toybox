#pragma once
#include "Math/Math.h"

namespace Toybox::Input
{
    bool IsGamepadButtonDown(const int id, const int button);
    bool IsGamepadButtonUp(const int id, const int button);
    bool IsGamepadButtonHeld(const int id, const int button);

    bool IsKeyDown(const int keyCode);
    bool IsKeyUp(const int keyCode);
    bool IsKeyHeld(const int keyCode);

    bool IsMouseButtonDown(const int button);
    bool IsMouseButtonUp(const int button);
    bool IsMouseButtonHeld(const int button);
    Math::Vector2 GetMousePosition();
}
