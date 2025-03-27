#pragma once
#include <tbxpch.h>
#include "Windowing/IWindow.h"
#include "Math/Vectors.h"

namespace Tbx
{
    class IInputHandler
    {
    public:
        IInputHandler() = default;
        virtual ~IInputHandler() = default;

        virtual void SetContext(const std::weak_ptr<IWindow>& windowToListenTo) = 0;

        virtual bool IsGamepadButtonDown(const int id, const int button) = 0;
        virtual bool IsGamepadButtonUp(const int id, const int button) = 0;
        virtual bool IsGamepadButtonHeld(const int id, const int button) = 0;

        virtual bool IsKeyDown(const int keyCode) = 0;
        virtual bool IsKeyUp(const int keyCode) = 0;
        virtual bool IsKeyHeld(const int keyCode) = 0;

        virtual bool IsMouseButtonDown(const int button) = 0;
        virtual bool IsMouseButtonUp(const int button) = 0;
        virtual bool IsMouseButtonHeld(const int button) = 0;
        virtual Vector2 GetMousePosition() = 0;
    };
}