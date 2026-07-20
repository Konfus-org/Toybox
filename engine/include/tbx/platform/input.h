#pragma once
#include "tbx/core/math.h"
#include "tbx/platform/keys.h"

// Polled input state — plain data + queries, so it is a namespace, not a class (state lives in
// input.cpp). Gameplay reads this each frame; the KeyEvent signal exists only for text/UI-style
// consumers. Main thread only; the platform backend feeds state during the pump.
namespace tbx::input
{
    /// @brief
    /// Purpose: Backend feed: records a key transition.
    void feed_key(Key key, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records a mouse button transition.
    void feed_mouse_button(MouseButton button, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records the pointer position and accumulates frame delta.
    void feed_mouse_move(Vec2 position, Vec2 delta);

    /// @brief
    /// Purpose: Backend feed: accumulates scroll wheel movement for the frame.
    void feed_scroll(float delta);

    /// @brief
    /// Purpose: Pointer movement accumulated this frame.
    Vec2 get_mouse_delta();

    /// @brief
    /// Purpose: Pointer position in window pixels.
    Vec2 get_mouse_position();

    /// @brief
    /// Purpose: Scroll wheel movement accumulated this frame.
    float get_scroll_delta();

    /// @brief
    /// Purpose: True while the key is held.
    bool is_down(Key key);

    /// @brief
    /// Purpose: True while the mouse button is held.
    bool is_mouse_down(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went down.
    bool is_mouse_pressed(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went up.
    bool is_mouse_released(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the key went down.
    bool is_pressed(Key key);

    /// @brief
    /// Purpose: True only on the frame the key went up.
    bool is_released(Key key);

    /// @brief
    /// Purpose: Rolls per-frame state; called by Engine::pump() before OS events feed in.
    void pump();
}
