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

    //// FRAME ////

    void update_input(InputState& input)
    {
        input.previous_keys = input.keys;
        input.previous_mouse = input.mouse;
        input.mouse_delta = Vec2(0.0f, 0.0f);
        input.scroll_delta = 0.0f;
    }

    void set_cursor_mode(InputState& input, const CursorMode mode)
    {
        input.cursor_mode = mode;
    }
}
