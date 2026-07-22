#pragma once
#include "tbx/math/math.h"
#include "tbx/platform/keys.h"
#include "tbx/utils/api.h"
#include "tbx/utils/typedefs.h"
#include <array>

// Polled input state — plain data + queries. Gameplay reads this each frame; the KeyEvent
// signal exists only for text/UI-style consumers. Main thread only; the platform backend
// feeds state during the pump.
namespace tbx::input
{
    /// @brief
    /// Purpose: The input module's whole state — one plain data blob held by value on the
    /// Runtime, main thread only.
    struct TBX_API InputState
    {
        std::array<bool, static_cast<size>(Key::COUNT)> keys = {};
        std::array<bool, static_cast<size>(Key::COUNT)> previous_keys = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> mouse = {};
        std::array<bool, static_cast<size>(MouseButton::COUNT)> previous_mouse = {};
        Vec2 mouse_position = Vec2(0.0f, 0.0f);
        Vec2 mouse_delta = Vec2(0.0f, 0.0f);
        float scroll_delta = 0.0f;
        CursorMode cursor_mode = CursorMode::NORMAL;
    };

    /// @brief
    /// Purpose: Backend feed: records a key transition.
    TBX_API void feed_key(InputState& input, Key key, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records a mouse button transition.
    TBX_API void feed_mouse_button(InputState& input, MouseButton button, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records the pointer position and accumulates frame delta.
    TBX_API void feed_mouse_move(InputState& input, Vec2 position, Vec2 delta);

    /// @brief
    /// Purpose: Backend feed: accumulates scroll wheel movement for the frame.
    TBX_API void feed_scroll(InputState& input, float delta);

    /// @brief
    /// Purpose: The cursor mode gameplay asked for (the platform backend applies it during
    /// the pump; headless windows ignore it).
    TBX_API CursorMode get_cursor_mode(const InputState& input);

    /// @brief
    /// Purpose: Pointer movement accumulated this frame.
    TBX_API Vec2 get_mouse_delta(const InputState& input);

    /// @brief
    /// Purpose: Pointer position in window pixels.
    TBX_API Vec2 get_mouse_position(const InputState& input);

    /// @brief
    /// Purpose: Scroll wheel movement accumulated this frame.
    TBX_API float get_scroll_delta(const InputState& input);

    /// @brief
    /// Purpose: True while the key is held.
    TBX_API bool is_down(const InputState& input, Key key);

    /// @brief
    /// Purpose: True while the mouse button is held.
    TBX_API bool is_mouse_down(const InputState& input, MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went down.
    TBX_API bool is_mouse_pressed(const InputState& input, MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the mouse button went up.
    TBX_API bool is_mouse_released(const InputState& input, MouseButton button);

    /// @brief
    /// Purpose: True only on the frame the key went down.
    TBX_API bool is_pressed(const InputState& input, Key key);

    /// @brief
    /// Purpose: True only on the frame the key went up.
    TBX_API bool is_released(const InputState& input, Key key);

    /// @brief
    /// Purpose: Rolls per-frame state (held becomes previous, deltas clear). Called by the
    /// runtime's pump before OS events feed in.
    TBX_API void pump(InputState& input);

    /// @brief
    /// Purpose: Asks for a cursor mode — NORMAL frees the pointer, LOCKED grabs it for
    /// mouse-look (deltas keep flowing). Takes effect at the next pump.
    TBX_API void set_cursor_mode(InputState& input, CursorMode mode);
}
