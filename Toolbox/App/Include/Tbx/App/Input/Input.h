#pragma once
#include "Tbx/App/Input/IInputHandler.h"
#include "Tbx/App/Windowing/IWindow.h"

namespace Tbx
{
    class Input
    {
    public:
        EXPORT static void Initialize();
        EXPORT static void Stop();
        EXPORT static void SetContext(const std::weak_ptr<IWindow>& context);

        EXPORT static bool IsGamepadButtonDown(const int id, const int button);
        EXPORT static bool IsGamepadButtonUp(const int id, const int button);
        EXPORT static bool IsGamepadButtonHeld(const int id, const int button);

        EXPORT static bool IsKeyDown(const int keyCode);
        EXPORT static bool IsKeyUp(const int keyCode);
        EXPORT static bool IsKeyHeld(const int keyCode);

        EXPORT static bool IsMouseButtonDown(const int button);
        EXPORT static bool IsMouseButtonUp(const int button);
        EXPORT static bool IsMouseButtonHeld(const int button);
        EXPORT static Vector2 GetMousePosition();

    private:
        static std::shared_ptr<IInputHandler> _handler;
        static std::weak_ptr<IWindow> _context;
    };
}
