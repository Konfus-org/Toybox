#include "tbx/platform/input.h"
#include "tbx/utils/typedefs.h"

namespace tbx
{
    static size index_of(const Key key)
    {
        return static_cast<size>(key);
    }

    static size index_of(const MouseButton button)
    {
        return static_cast<size>(button);
    }

    static size index_of(const GamepadButton button)
    {
        return static_cast<size>(button);
    }

    static size index_of(const GamepadAxis axis)
    {
        return static_cast<size>(axis);
    }

    static bool is_valid_gamepad(const int slot)
    {
        return slot >= 0 && slot < MAX_GAMEPADS;
    }

    //// QUERIES ////
    // The backend writes InputState directly during update_input; these are the read side.

    CursorMode get_cursor_mode(const InputState& input)
    {
        return input.cursor_mode;
    }

    void set_cursor_mode(InputState& input, const CursorMode mode)
    {
        input.cursor_mode = mode;
    }

    bool is_down(const InputState& input, const Key key)
    {
        return input.keys[index_of(key)];
    }

    bool is_down(const InputState& input, const MouseButton button)
    {
        return input.mouse[index_of(button)];
    }

    bool is_down(const InputState& input, const GamepadButton button, const int slot)
    {
        return is_valid_gamepad(slot)
            && input.gamepads[static_cast<size>(slot)].buttons[index_of(button)];
    }

    bool is_pressed(const InputState& input, const Key key)
    {
        return input.keys[index_of(key)] && !input.previous_keys[index_of(key)];
    }

    bool is_pressed(const InputState& input, const MouseButton button)
    {
        return input.mouse[index_of(button)] && !input.previous_mouse[index_of(button)];
    }

    bool is_pressed(const InputState& input, const GamepadButton button, const int slot)
    {
        if (!is_valid_gamepad(slot))
            return false;
        const GamepadState& gamepad = input.gamepads[static_cast<size>(slot)];
        return gamepad.buttons[index_of(button)] && !gamepad.previous_buttons[index_of(button)];
    }

    bool is_released(const InputState& input, const Key key)
    {
        return !input.keys[index_of(key)] && input.previous_keys[index_of(key)];
    }

    bool is_released(const InputState& input, const MouseButton button)
    {
        return !input.mouse[index_of(button)] && input.previous_mouse[index_of(button)];
    }

    bool is_released(const InputState& input, const GamepadButton button, const int slot)
    {
        if (!is_valid_gamepad(slot))
            return false;
        const GamepadState& gamepad = input.gamepads[static_cast<size>(slot)];
        return !gamepad.buttons[index_of(button)] && gamepad.previous_buttons[index_of(button)];
    }

    bool is_gamepad_connected(const InputState& input, const int slot)
    {
        return is_valid_gamepad(slot) && input.gamepads[static_cast<size>(slot)].is_connected;
    }

    float get_axis(const InputState& input, const GamepadAxis axis, const int slot)
    {
        if (!is_valid_gamepad(slot))
            return 0.0f;
        return input.gamepads[static_cast<size>(slot)].axes[index_of(axis)];
    }

    float get_axis(const InputState& input, const MouseAxis axis)
    {
        switch (axis)
        {
            case MouseAxis::X:
                return input.mouse_position.x;
            case MouseAxis::Y:
                return input.mouse_position.y;
            default:
                return 0.0f; // SCROLL is delta-only
        }
    }

    float get_axis_delta(const InputState& input, const MouseAxis axis)
    {
        switch (axis)
        {
            case MouseAxis::X:
                return input.mouse_delta.x;
            case MouseAxis::Y:
                return input.mouse_delta.y;
            case MouseAxis::SCROLL:
                return input.scroll_delta;
            default:
                return 0.0f;
        }
    }
}
