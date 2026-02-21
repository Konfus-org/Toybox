#pragma once
#include "tbx/math/vectors.h"
#include "tbx/messages/message.h"
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    /// <summary>Represents a snapshot of keyboard input for the current frame.</summary>
    /// <remarks>Purpose: Stores pressed key data used to evaluate input actions.
    /// Ownership: Value type; owns copied key codes.
    /// Thread Safety: Safe for concurrent read access after construction.</remarks>
    struct TBX_API KeyboardState
    {
        std::unordered_set<int> pressed_keys = {};
    };

    /// <summary>Represents a snapshot of mouse input for the current frame.</summary>
    /// <remarks>Purpose: Stores button and pointer information used by input actions.
    /// Ownership: Value type; owns copied button code data.
    /// Thread Safety: Safe for concurrent read access after construction.</remarks>
    struct TBX_API MouseState
    {
        std::unordered_set<int> pressed_buttons = {};
        Vec2 position = {};
        Vec2 delta = {};
        float wheel_delta = 0.0F;
    };

    /// <summary>Represents the active mouse lock behavior selected by the input backend.</summary>
    /// <remarks>Purpose: Exposes whether mouse input is unlocked, relative, or window-grabbed.
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.</remarks>
    enum class MouseLockMode
    {
        UNLOCKED,
        RELATIVE,
        INPUT_GRABBED
    };

    /// <summary>Represents a snapshot of a single controller for the current frame.</summary>
    /// <remarks>Purpose: Stores button and axis values used by input actions.
    /// Ownership: Value type; owns copied button and axis values.
    /// Thread Safety: Safe for concurrent read access after construction.</remarks>
    struct TBX_API ControllerState
    {
        bool is_connected = false;
        int controller_index = -1;
        std::unordered_set<int> pressed_buttons = {};
        std::unordered_map<int, float> axis_values = {};
    };

    /// <summary>Requests a keyboard state snapshot from the active input backend.</summary>
    /// <remarks>Purpose: Decouples gameplay systems from backend-specific keyboard APIs.
    /// Ownership: The request owns the response value.
    /// Thread Safety: Can be sent from any thread; backend handling is
    /// implementation-defined.</remarks>
    struct TBX_API KeyboardStateRequest : public Request<KeyboardState>
    {
    };

    /// <summary>Requests a mouse state snapshot from the active input backend.</summary>
    /// <remarks>Purpose: Decouples gameplay systems from backend-specific mouse APIs.
    /// Ownership: The request owns the response value.
    /// Thread Safety: Can be sent from any thread; backend handling is
    /// implementation-defined.</remarks>
    struct TBX_API MouseStateRequest : public Request<MouseState>
    {
    };

    /// <summary>Requests that the input backend apply a specific mouse lock mode.</summary>
    /// <remarks>Purpose: Allows gameplay code to explicitly choose mouse lock behavior.
    /// Ownership: The request owns the requested mode value.
    /// Thread Safety: Can be sent from any thread; backend handling is implementation-defined.
    /// </remarks>
    struct TBX_API SetMouseLockRequest : public Request<void>
    {
        /// <summary>Creates a lock request for the desired lock mode.</summary>
        /// <remarks>Purpose: Captures the mouse lock mode that should be applied.
        /// Ownership: Copies the enum value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        explicit SetMouseLockRequest(MouseLockMode requested_lock_mode)
            : mode(requested_lock_mode)
        {
        }

        MouseLockMode mode = MouseLockMode::UNLOCKED;
    };

    /// <summary>Requests the currently effective mouse lock mode from the input backend.</summary>
    /// <remarks>Purpose: Allows systems to inspect whether mouse lock is active and how it is
    /// applied. Ownership: The request owns the response value. Thread Safety: Can be sent from any
    /// thread; backend handling is implementation-defined.</remarks>
    struct TBX_API MouseLockModeRequest : public Request<MouseLockMode>
    {
    };

    /// <summary>Requests a controller state snapshot for a specific player/controller
    /// index.</summary> <remarks>Purpose: Decouples gameplay systems from backend-specific
    /// controller APIs. Ownership: The request owns the response value and copied index. Thread
    /// Safety: Can be sent from any thread; backend handling is implementation-defined.</remarks>
    struct TBX_API ControllerStateRequest : public Request<ControllerState>
    {
        /// <summary>Creates a request for the given controller index.</summary>
        /// <remarks>Purpose: Identifies the controller to query.
        /// Ownership: Copies the index value.
        /// Thread Safety: Safe to construct on any thread.</remarks>
        explicit ControllerStateRequest(int requested_index)
            : controller_index(requested_index)
        {
        }

        int controller_index = -1;
    };
}
