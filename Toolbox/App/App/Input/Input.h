#pragma once
#include "App/Input/IInputHandler.h"
#include "App/Windowing/IWindow.h"

namespace Tbx
{
    class Input
    {
    public:
        TBX_API static void Initialize();
        TBX_API static void Stop();
        TBX_API static void SetContext(const std::weak_ptr<IWindow>& context);

        TBX_API static bool IsGamepadButtonDown(const int id, const int button);
        TBX_API static bool IsGamepadButtonUp(const int id, const int button);
        TBX_API static bool IsGamepadButtonHeld(const int id, const int button);

        TBX_API static bool IsKeyDown(const int keyCode);
        TBX_API static bool IsKeyUp(const int keyCode);
        TBX_API static bool IsKeyHeld(const int keyCode);

        TBX_API static bool IsMouseButtonDown(const int button);
        TBX_API static bool IsMouseButtonUp(const int button);
        TBX_API static bool IsMouseButtonHeld(const int button);
        TBX_API static Vector2 GetMousePosition();

    private:
        static std::shared_ptr<IInputHandler> _handler;
        static std::weak_ptr<IWindow> _context;
    };
}
