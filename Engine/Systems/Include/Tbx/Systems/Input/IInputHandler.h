#pragma once
#include "Tbx/Systems/Windowing/IWindow.h"
#include <Tbx/Math/Vectors.h>
#include <memory>


namespace Tbx
{
    class IInputHandler
    {
    public:
        virtual ~IInputHandler() = default;

        virtual void Initialize(const std::shared_ptr<IWindow>& windowToListenTo) = 0;

        virtual bool IsGamepadButtonDown(const int gamepadId, const int button) const = 0;
        virtual bool IsGamepadButtonUp(const int gamepadId, const int button) const = 0;
        virtual bool IsGamepadButtonHeld(const int gamepadId, const int button) const = 0;

        virtual bool IsKeyDown(const int keyCode) const = 0;
        virtual bool IsKeyUp(const int keyCode) const = 0;
        virtual bool IsKeyHeld(const int keyCode) const = 0;

        virtual bool IsMouseButtonDown(const int button) const = 0;
        virtual bool IsMouseButtonUp(const int button) const = 0;
        virtual bool IsMouseButtonHeld(const int button) const = 0;
        virtual Vector2 GetMousePosition() const = 0;
    };
}