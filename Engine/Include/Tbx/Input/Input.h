#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    class Input
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();

        EXPORT static bool IsGamepadButtonDown(const int gamepadId, const int button);
        EXPORT static bool IsGamepadButtonUp(const int gamepadId, const int button);
        EXPORT static bool IsGamepadButtonHeld(const int gamepadId, const int button);

        EXPORT static bool IsKeyDown(const int keyCode);
        EXPORT static bool IsKeyUp(const int keyCode);
        EXPORT static bool IsKeyHeld(const int keyCode);

        EXPORT static bool IsMouseButtonDown(const int button);
        EXPORT static bool IsMouseButtonUp(const int button);
        EXPORT static bool IsMouseButtonHeld(const int button);
        EXPORT static Vector2 GetMousePosition();

    private:
        static void OnWindowFocusChanged(const WindowFocusedEvent& args);

        static UID _windowFocusChangedEventId;
    };
}
