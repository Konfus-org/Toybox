#pragma once
#include "tbx/math/vectors.h"
#include "tbx/messages/message.h"
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    /// @brief
    /// Purpose: Stores pressed key data used to evaluate input actions.
    /// @details
    /// Ownership: Value type; owns copied key codes.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API KeyboardState
    {
        std::unordered_set<int> pressed_keys = {};
    };

    /// @brief
    /// Purpose: Stores button and pointer information used by input actions.
    /// @details
    /// Ownership: Value type; owns copied button code data.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API MouseState
    {
        std::unordered_set<int> pressed_buttons = {};
        Vec2 position = {};
        Vec2 delta = {};
        float wheel_delta = 0.0F;
    };

    /// @brief
    /// Purpose: Exposes whether mouse input is unlocked, relative, or window-grabbed.
    /// @details
    /// Ownership: Enum value type with no ownership semantics.
    /// Thread Safety: Safe for concurrent use.
    enum class MouseLockMode
    {
        UNLOCKED,
        RELATIVE,
        INPUT_GRABBED
    };

    /// @brief
    /// Purpose: Stores button and axis values used by input actions.
    /// @details
    /// Ownership: Value type; owns copied button and axis values.
    /// Thread Safety: Safe for concurrent read access after construction.
    struct TBX_API ControllerState
    {
        bool is_connected = false;
        int controller_index = -1;
        std::unordered_set<int> pressed_buttons = {};
        std::unordered_map<int, float> axis_values = {};
    };

    /// @brief
    /// Purpose: Decouples gameplay systems from backend-specific keyboard APIs.
    /// @details
    /// Ownership: The request owns the response value.
    /// Thread Safety: Can be sent from any thread; backend handling is
    struct TBX_API KeyboardStateRequest : public Request<KeyboardState>
    {
    };

    /// @brief
    /// Purpose: Decouples gameplay systems from backend-specific mouse APIs.
    /// @details
    /// Ownership: The request owns the response value.
    /// Thread Safety: Can be sent from any thread; backend handling is
    struct TBX_API MouseStateRequest : public Request<MouseState>
    {
    };

    /// @brief
    /// Purpose: Allows gameplay code to explicitly choose mouse lock behavior.
    /// @details
    /// Ownership: The request owns the requested mode value.
    /// Thread Safety: Can be sent from any thread; backend handling is implementation-defined.
    struct TBX_API SetMouseLockRequest : public Request<void>
    {
        SetMouseLockRequest(MouseLockMode requested_lock_mode)
            : mode(requested_lock_mode)
        {
        }

        MouseLockMode mode = MouseLockMode::UNLOCKED;
    };

    /// @brief Requests the currently effective mouse lock mode from the input backend.
    /// @details Purpose: Allows systems to inspect whether mouse lock is active and how it is
    /// applied. Ownership: The request owns the response value. Thread Safety: Can be sent from any
    /// thread; backend handling is implementation-defined.
    struct TBX_API MouseLockModeRequest : public Request<MouseLockMode>
    {
    };

    /// @brief Requests a controller state snapshot for a specific player/controller index.
    /// @details Purpose: Decouples gameplay systems from backend-specific
    /// controller APIs. Ownership: The request owns the response value and copied index. Thread
    /// Safety: Can be sent from any thread; backend handling is implementation-defined.
    struct TBX_API ControllerStateRequest : public Request<ControllerState>
    {
        ControllerStateRequest(int requested_index)
            : controller_index(requested_index)
        {
        }

        int controller_index = -1;
    };
}
