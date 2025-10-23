#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Math/Vectors.h"

namespace Tbx
{
    class TBX_EXPORT IInputHandler
    {
    public:
        virtual ~IInputHandler();

        /// <summary>
        /// Updates the states of connected input devices.
        /// Should be called once per frame.
        /// </summary>
        virtual void Update() = 0;

        virtual bool IsGamepadButtonDown(int playerIndex, int button) const = 0;
        virtual bool IsGamepadButtonUp(int playerIndex, int button) const = 0;
        virtual bool IsGamepadButtonHeld(int playerIndex, int button) const = 0;
        virtual float GetGamepadAxis(int playerIndex, int axis) const = 0;

        virtual bool IsKeyDown(int keyCode) const = 0;
        virtual bool IsKeyUp(int keyCode) const = 0;
        virtual bool IsKeyHeld(int keyCode) const = 0;

        virtual bool IsMouseButtonDown(int button) const = 0;
        virtual bool IsMouseButtonUp(int button) const = 0;
        virtual bool IsMouseButtonHeld(int button) const = 0;
        virtual Vector2 GetMousePosition() const = 0;
        virtual Vector2 GetMouseDelta() const = 0;
    };
}