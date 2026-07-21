#include "tbx/platform/input.h"
#include "tbx/utils/typedefs.h"
#include <array>

namespace tbx::input
{
    /// @brief
    /// Purpose: The module's whole state — one plain data blob, main thread only.
    struct InputState
    {
        std::array<bool, static_cast<size>(Key::COUNT)> keys = {};
        std::array<bool, static_cast<size>(Key::COUNT)> previous_keys = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> mouse = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> previous_mouse = {};
        Vec2 mouse_position = Vec2(0.0f, 0.0f);
        Vec2 mouse_delta = Vec2(0.0f, 0.0f);
        float scroll_delta = 0.0f;
    };

    static InputState g_state = {};

    static size index_of(Key key)
    {
        return static_cast<size>(key);
    }

    static size index_of(MouseButton button)
    {
        return static_cast<size>(button);
    }

    //// FEED ////

    void feed_key(Key key, bool is_down)
    {
        g_state.keys[index_of(key)] = is_down;
    }

    void feed_mouse_button(MouseButton button, bool is_down)
    {
        g_state.mouse[index_of(button)] = is_down;
    }

    void feed_mouse_move(Vec2 position, Vec2 delta)
    {
        g_state.mouse_position = position;
        g_state.mouse_delta += delta;
    }

    void feed_scroll(float delta)
    {
        g_state.scroll_delta += delta;
    }

    //// QUERIES ////

    Vec2 get_mouse_delta()
    {
        return g_state.mouse_delta;
    }

    Vec2 get_mouse_position()
    {
        return g_state.mouse_position;
    }

    float get_scroll_delta()
    {
        return g_state.scroll_delta;
    }

    bool is_down(Key key)
    {
        return g_state.keys[index_of(key)];
    }

    bool is_mouse_down(MouseButton button)
    {
        return g_state.mouse[index_of(button)];
    }

    bool is_mouse_pressed(MouseButton button)
    {
        return g_state.mouse[index_of(button)] && !g_state.previous_mouse[index_of(button)];
    }

    bool is_mouse_released(MouseButton button)
    {
        return !g_state.mouse[index_of(button)] && g_state.previous_mouse[index_of(button)];
    }

    bool is_pressed(Key key)
    {
        return g_state.keys[index_of(key)] && !g_state.previous_keys[index_of(key)];
    }

    bool is_released(Key key)
    {
        return !g_state.keys[index_of(key)] && g_state.previous_keys[index_of(key)];
    }

    //// FRAME ////

    void pump()
    {
        g_state.previous_keys = g_state.keys;
        g_state.previous_mouse = g_state.mouse;
        g_state.mouse_delta = Vec2(0.0f, 0.0f);
        g_state.scroll_delta = 0.0f;
    }
}
