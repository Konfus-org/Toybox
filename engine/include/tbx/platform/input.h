#pragma once
#include "tbx/utils/api.h"
#include "tbx/math/math.h"
#include "tbx/platform/keys.h"

// Polled input state — plain data + queries, so it is a namespace, not a class (state lives in
// input.cpp). Gameplay reads this each frame; the KeyEvent signal exists only for text/UI-style
// consumers. Main thread only; the platform backend feeds state during the pump.
namespace tbx::input
{
    /// @brief
    /// Purpose: Backend feed: records a key transition.
    TBX_API void feed_key(Key key, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records a mouse button transition.
    TBX_API void feed_mouse_button(MouseButton button, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records the pointer position and accumulates frame delta.
    TBX_API void feed_mouse_move(Vec2 position, Vec2 delta);

    /// @brief
    /// Purpose: Backend feed: accumulates scroll wheel movement for the frame.
    TBX_API void feed_scroll(float delta);

    /// @brief
    /// Purpose: The cursor mode gameplay asked for (the platform backend applies it during
    /// the pump; headless windows ignore it).
    TBX_API CursorMode get_cursor_mode();

    /// @brief
    /// Purpose: Pointer movement accumulated this frame.
    TBX_API Vec2 get_mouse_delta();

    /// @brief
    /// Purpose: Pointer position in window pixels.
    TBX_API Vec2 get_mouse_position();

    /// @brief
    /// Purpose: Scroll wheel movement accumulated this frame.
    TBX_API float get_scroll_delta();

    /// @brief
    /// Purpose: True while the key is held.
    TBX_API bool is_down(Key key);

    /// @brief
    /// Purpose: True while the mouse button is held.
    TBX_API bool is_mouse_down(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went down.
    TBX_API bool is_mouse_pressed(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went up.
    TBX_API bool is_mouse_released(MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the key went down.
    TBX_API bool is_pressed(Key key);

    /// @brief
    /// Purpose: True only on the frame the key went up.
    TBX_API bool is_released(Key key);

    /// @brief
    /// Purpose: Rolls per-frame state; called by Engine::pump() before OS events feed in.
    TBX_API void pump();

    /// @brief
    /// Purpose: Asks for a cursor mode — NORMAL frees the pointer, LOCKED grabs it for
    /// mouse-look (deltas keep flowing). Takes effect at the next pump.
    TBX_API void set_cursor_mode(CursorMode mode);
}
