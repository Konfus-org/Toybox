#pragma once
#include "Tbx/App/Input/IInputHandler.h"
#include "Tbx/App/Windowing/IWindow.h"
#include "Tbx/App/Events/WindowEvents.h"

namespace Tbx
{
    class Input
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Shutdown();

        // TODO: reimplement gamepad support via events
        ////EXPORT static bool IsGamepadButtonDown(const int id, const int button);
        ////EXPORT static bool IsGamepadButtonUp(const int id, const int button);
        ////EXPORT static bool IsGamepadButtonHeld(const int id, const int button);

        EXPORT static bool IsKeyDown(const int keyCode);
        EXPORT static bool IsKeyUp(const int keyCode);

        // TODO: reimplement key held support via events
        //EXPORT static bool IsKeyHeld(const int keyCode);

        // TODO: reimplement mouse support via events
        ////EXPORT static bool IsMouseButtonDown(const int button);
        ////EXPORT static bool IsMouseButtonUp(const int button);
        ////EXPORT static bool IsMouseButtonHeld(const int button);
        ////EXPORT static Vector2 GetMousePosition();

    private:
        static UID _windowFocusChangedEventId;

        static void OnWindowFocusChanged(const WindowFocusChangedEvent& args);
    };
}
