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
    class Input
    {
    public:
        /// <summary>
        /// Initializes the global input handler that backs all static queries.
        /// Must be called once before any other Input methods are used.
        /// </summary>
        EXPORT static void Initialize(const Tbx::Ref<IInputHandler>& inputHandler);
        /// <summary>
        /// Polls the currently configured input handler for fresh state.
        /// </summary>
        EXPORT static void Update();
        /// <summary>
        /// Releases the active input handler and returns the system to an uninitialized state.
        /// </summary>
        EXPORT static void Shutdown();

        /// <summary>
        /// Returns whether a gamepad button was pressed during the current frame.
        /// </summary>
        EXPORT static bool IsGamepadButtonDown(const int playerIndex, const int button);
        /// <summary>
        /// Returns whether a gamepad button was released during the current frame.
        /// </summary>
        EXPORT static bool IsGamepadButtonUp(const int playerIndex, const int button);
        /// <summary>
        /// Returns whether a gamepad button is currently being held.
        /// </summary>
        EXPORT static bool IsGamepadButtonHeld(const int playerIndex, const int button);
        /// <summary>
        /// Reads the specified gamepad axis value.
        /// </summary>
        EXPORT static float GetGamepadAxis(const int playerIndex, const int axis);

        /// <summary>
        /// Returns whether a keyboard key was pressed during the current frame.
        /// </summary>
        EXPORT static bool IsKeyDown(const int keyCode);
        /// <summary>
        /// Returns whether a keyboard key was released during the current frame.
        /// </summary>
        EXPORT static bool IsKeyUp(const int keyCode);
        /// <summary>
        /// Returns whether a keyboard key is currently being held.
        /// </summary>
        EXPORT static bool IsKeyHeld(const int keyCode);

        /// <summary>
        /// Returns whether a mouse button was pressed during the current frame.
        /// </summary>
        EXPORT static bool IsMouseButtonDown(const int button);
        /// <summary>
        /// Returns whether a mouse button was released during the current frame.
        /// </summary>
        EXPORT static bool IsMouseButtonUp(const int button);
        /// <summary>
        /// Returns whether a mouse button is currently being held.
        /// </summary>
        EXPORT static bool IsMouseButtonHeld(const int button);
        /// <summary>
        /// Retrieves the current mouse position in screen space.
        /// </summary>
        EXPORT static Vector2 GetMousePosition();
        /// <summary>
        /// Retrieves the mouse delta since the previous frame.
        /// </summary>
        EXPORT static Vector2 GetMouseDelta();

    private:
        /// <summary>
        /// Ensures the input handler has been configured before use.
        /// </summary>
        static bool EnsureHandler();

    private:
        static Tbx::Ref<IInputHandler> _inputHandler;
    };
}

