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

    static bool is_valid_gamepad(const int index)
    {
        return index >= 0 && index < MAX_GAMEPADS;
    }

    //// FEED ////

    void feed_key(InputState& input, const Key key, const bool is_down)
    {
        input.keys[index_of(key)] = is_down;
    }

    void feed_mouse_button(InputState& input, const MouseButton button, const bool is_down)
    {
        input.mouse[index_of(button)] = is_down;
    }

    void feed_mouse_move(InputState& input, const Vec2 position, const Vec2 delta)
    {
        input.mouse_position = position;
        input.mouse_delta += delta;
    }

    void feed_scroll(InputState& input, const float delta)
    {
        input.scroll_delta += delta;
    }

    void feed_gamepad_connected(InputState& input, const int index, const bool is_connected)
    {
        if (!is_valid_gamepad(index))
            return;
        GamepadState& gamepad = input.gamepads[static_cast<size>(index)];
        gamepad.is_connected = is_connected;
        if (!is_connected)
        {
            // A vacated slot must not report stale held buttons or off-center sticks.
            gamepad.buttons = {};
            gamepad.previous_buttons = {};
            gamepad.axes = {};
        }
    }

    void feed_gamepad_button(
        InputState& input,
        const int index,
        const GamepadButton button,
        const bool is_down)
    {
        if (!is_valid_gamepad(index))
            return;
        input.gamepads[static_cast<size>(index)].buttons[index_of(button)] = is_down;
    }

    void feed_gamepad_axis(
        InputState& input,
        const int index,
        const GamepadAxis axis,
        const float value)
    {
        if (!is_valid_gamepad(index))
            return;
        input.gamepads[static_cast<size>(index)].axes[index_of(axis)] = value;
    }

    //// QUERIES ////

    CursorMode get_cursor_mode(const InputState& input)
    {
        return input.cursor_mode;
    }

    Vec2 get_mouse_delta(const InputState& input)
    {
        return input.mouse_delta;
    }

    Vec2 get_mouse_position(const InputState& input)
    {
        return input.mouse_position;
    }

    float get_scroll_delta(const InputState& input)
    {
        return input.scroll_delta;
    }

    bool is_down(const InputState& input, const Key key)
    {
        return input.keys[index_of(key)];
    }

    bool is_mouse_down(const InputState& input, const MouseButton button)
    {
        return input.mouse[index_of(button)];
    }

    bool is_mouse_pressed(const InputState& input, const MouseButton button)
    {
        return input.mouse[index_of(button)] && !input.previous_mouse[index_of(button)];
    }

    bool is_mouse_released(const InputState& input, const MouseButton button)
    {
        return !input.mouse[index_of(button)] && input.previous_mouse[index_of(button)];
    }

    bool is_pressed(const InputState& input, const Key key)
    {
        return input.keys[index_of(key)] && !input.previous_keys[index_of(key)];
    }

    bool is_released(const InputState& input, const Key key)
    {
        return !input.keys[index_of(key)] && input.previous_keys[index_of(key)];
    }

    bool is_gamepad_connected(const InputState& input, const int index)
    {
        return is_valid_gamepad(index) && input.gamepads[static_cast<size>(index)].is_connected;
    }

    bool is_gamepad_down(const InputState& input, const int index, const GamepadButton button)
    {
        return is_valid_gamepad(index)
            && input.gamepads[static_cast<size>(index)].buttons[index_of(button)];
    }

    bool is_gamepad_pressed(const InputState& input, const int index, const GamepadButton button)
    {
        if (!is_valid_gamepad(index))
            return false;
        const GamepadState& gamepad = input.gamepads[static_cast<size>(index)];
        return gamepad.buttons[index_of(button)] && !gamepad.previous_buttons[index_of(button)];
    }

    bool is_gamepad_released(const InputState& input, const int index, const GamepadButton button)
    {
        if (!is_valid_gamepad(index))
            return false;
        const GamepadState& gamepad = input.gamepads[static_cast<size>(index)];
        return !gamepad.buttons[index_of(button)] && gamepad.previous_buttons[index_of(button)];
    }

    float get_gamepad_axis(const InputState& input, const int index, const GamepadAxis axis)
    {
        if (!is_valid_gamepad(index))
            return 0.0f;
        return input.gamepads[static_cast<size>(index)].axes[index_of(axis)];
    }

    //// FRAME ////

    void advance_input_frame(InputState& input)
    {
        input.previous_keys = input.keys;
        input.previous_mouse = input.mouse;
        input.mouse_delta = Vec2(0.0f, 0.0f);
        input.scroll_delta = 0.0f;
        // Buttons roll for edge queries; axes are absolute levels, so they persist untouched.
        for (GamepadState& gamepad : input.gamepads)
            gamepad.previous_buttons = gamepad.buttons;
    }

    void set_cursor_mode(InputState& input, const CursorMode mode)
    {
        input.cursor_mode = mode;
    }
}
