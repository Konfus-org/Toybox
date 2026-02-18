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
