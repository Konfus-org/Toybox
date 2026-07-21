#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/vectors.h"
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
    /// Purpose: Backend interface implemented by input plugins: raw device polling that the
    /// engine's InputManager reads to build each frame's input snapshot. Mirrors the
    /// window/graphics/physics backend split, keeping device specifics in a plugin while the
    /// manager stays engine-owned.
    /// @details
    /// Ownership: Implementations own backend/device resources. Thread Safety: Not thread-safe;
    /// drive from the update thread.
    class TBX_API IInputBackend
    {
      public:
        virtual ~IInputBackend() noexcept = default;

        virtual KeyboardState get_keyboard_state() const = 0;
        virtual ControllerState get_controller_state(int controller_index) const = 0;
        virtual MouseState get_mouse_state() const = 0;
        virtual void set_mouse_lock_mode(MouseLockMode mode) = 0;
        virtual MouseLockMode get_mouse_lock_mode() const = 0;
        virtual void update() = 0;
    };
}
