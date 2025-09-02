#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    class Input
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Update();
        EXPORT static void Shutdown();

        EXPORT static bool IsGamepadButtonDown(const int playerIndex, const int button);
        EXPORT static bool IsGamepadButtonUp(const int playerIndex, const int button);
        EXPORT static bool IsGamepadButtonHeld(const int playerIndex, const int button);
        EXPORT static float GetGamepadAxis(const int playerIndex, const int axis);

        EXPORT static bool IsKeyDown(const int keyCode);
        EXPORT static bool IsKeyUp(const int keyCode);
        EXPORT static bool IsKeyHeld(const int keyCode);

        EXPORT static bool IsMouseButtonDown(const int button);
        EXPORT static bool IsMouseButtonUp(const int button);
        EXPORT static bool IsMouseButtonHeld(const int button);
        EXPORT static Vector2 GetMousePosition();
        EXPORT static Vector2 GetMouseDelta();

    private:
        static std::weak_ptr<IInputHandlerPlugin> _inputHandler;
    };
}
