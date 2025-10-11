#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Provides static convenience helpers for interacting with the engine input layer.
    /// </summary>
    class TBX_EXPORT Input
    {
    public:
        /// <summary>
        /// Initializes the global input handler that backs all static queries.
        /// Must be called once before any other Input methods are used.
        /// </summary>
        static void SetHandler(const Ref<IInputHandler>& inputHandler);

        /// <summary>
        /// Releases the active input handler and returns the system to an uninitialized state.
        /// </summary>
        static void ClearHandler();

        /// <summary>
        /// Polls the currently configured input handler for fresh state.
        /// </summary>
        static void Update();

        /// <summary>
        /// Returns whether a gamepad button was pressed during the current frame.
        /// </summary>
        static bool IsGamepadButtonDown(const int playerIndex, const int button);

        /// <summary>
        /// Returns whether a gamepad button was released during the current frame.
        /// </summary>
        static bool IsGamepadButtonUp(const int playerIndex, const int button);

        /// <summary>
        /// Returns whether a gamepad button is currently being held.
        /// </summary>
        static bool IsGamepadButtonHeld(const int playerIndex, const int button);

        /// <summary>
        /// Reads the specified gamepad axis value.
        /// </summary>
        static float GetGamepadAxis(const int playerIndex, const int axis);

        /// <summary>
        /// Returns whether a keyboard key was pressed during the current frame.
        /// </summary>
        static bool IsKeyDown(const int keyCode);

        /// <summary>
        /// Returns whether a keyboard key was released during the current frame.
        /// </summary>
        static bool IsKeyUp(const int keyCode);

        /// <summary>
        /// Returns whether a keyboard key is currently being held.
        /// </summary>
        static bool IsKeyHeld(const int keyCode);

        /// <summary>
        /// Returns whether a mouse button was pressed during the current frame.
        /// </summary>
        static bool IsMouseButtonDown(const int button);

        /// <summary>
        /// Returns whether a mouse button was released during the current frame.
        /// </summary>
        static bool IsMouseButtonUp(const int button);

        /// <summary>
        /// Returns whether a mouse button is currently being held.
        /// </summary>
        static bool IsMouseButtonHeld(const int button);

        /// <summary>
        /// Retrieves the current mouse position in screen space.
        /// </summary>
        static Vector2 GetMousePosition();

        /// <summary>
        /// Retrieves the mouse delta since the previous frame.
        /// </summary>
        static Vector2 GetMouseDelta();

    private:
        /// <summary>
        /// Ensures the input handler has been configured before use.
        /// </summary>
        static bool EnsureHandler();

    private:
        static Ref<IInputHandler> _inputHandler;
    };
}

