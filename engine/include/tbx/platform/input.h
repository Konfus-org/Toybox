#pragma once
#include "tbx/api.h"
#include "tbx/math/math.h"
#include "tbx/platform/keys.h"
#include "tbx/utils/typedefs.h"
#include <array>

// Polled input state — plain data + queries. Gameplay reads this each frame; the KeyEvent
// signal exists only for text/UI-style consumers. Main thread only; the platform backend
// feeds state during the pump.
namespace tbx
{
    struct EventsState; // pumped key transitions emit through the events module

    /// @brief
    /// Purpose: How many controllers the input state tracks at once (splitscreen coop). Pads
    /// connect into the first free slot [0, MAX_GAMEPADS); gameplay reads a slot by its index.
    inline constexpr int MAX_GAMEPADS = 4;

    /// @brief
    /// Purpose: One controller's state as plain data. Buttons carry a previous frame for edge
    /// queries (like keys/mouse); axes are absolute levels, so they have no previous and are not
    /// cleared on roll. is_connected is false for an empty slot.
    struct TBX_API GamepadState
    {
        bool is_connected = false;
        std::array<bool, static_cast<size>(GamepadButton::COUNT)> buttons = {};
        std::array<bool, static_cast<size>(GamepadButton::COUNT)> previous_buttons = {};
        std::array<float, static_cast<size>(GamepadAxis::COUNT)> axes = {};
    };

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
        std::array<GamepadState, MAX_GAMEPADS> gamepads = {};
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
    /// Purpose: Backend feed: marks a controller slot connected or disconnected. Disconnecting
    /// clears the slot's buttons and axes. Out-of-range indices are ignored.
    TBX_API void feed_gamepad_connected(InputState& input, int index, bool is_connected);

    /// @brief
    /// Purpose: Backend feed: records a controller button transition. Out-of-range indices are
    /// ignored.
    TBX_API void feed_gamepad_button(InputState& input, int index, GamepadButton button, bool is_down);

    /// @brief
    /// Purpose: Backend feed: records a controller axis level (sticks in [-1, 1], triggers in
    /// [0, 1]). Out-of-range indices are ignored.
    TBX_API void feed_gamepad_axis(InputState& input, int index, GamepadAxis axis, float value);

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
    /// Purpose: True while a controller is plugged into this slot. Out-of-range indices read
    /// false.
    TBX_API bool is_gamepad_connected(const InputState& input, int index);

    /// @brief
    /// Purpose: True while the controller button is held. Out-of-range indices read false.
    TBX_API bool is_gamepad_down(const InputState& input, int index, GamepadButton button);

    /// @brief
    /// Purpose: True only on the frame the controller button went down. Out-of-range indices
    /// read false.
    TBX_API bool is_gamepad_pressed(const InputState& input, int index, GamepadButton button);

    /// @brief
    /// Purpose: True only on the frame the controller button went up. Out-of-range indices read
    /// false.
    TBX_API bool is_gamepad_released(const InputState& input, int index, GamepadButton button);

    /// @brief
    /// Purpose: The controller axis level (sticks in [-1, 1], triggers in [0, 1]). Out-of-range
    /// indices read 0. Callers apply their own deadzone.
    TBX_API float get_gamepad_axis(const InputState& input, int index, GamepadAxis axis);

    /// @brief
    /// Purpose: Rolls per-frame state (held becomes previous, deltas clear). Backend-agnostic;
    /// the platform backend's update_input() calls it before OS events feed in.
    TBX_API void advance_input_frame(InputState& input);

    /// @brief
    /// Purpose: The per-frame input pass: rolls frame state, then pumps OS input events
    /// (keyboard, mouse, controllers) into the input state, emitting key transitions through
    /// events. Implemented by the platform backend; headless runs roll but skip the pump.
    TBX_API void update_input(InputState& input, EventsState& events);

    /// @brief
    /// Purpose: Asks for a cursor mode — NORMAL frees the pointer, LOCKED grabs it for
    /// mouse-look (deltas keep flowing). Takes effect at the next pump.
    TBX_API void set_cursor_mode(InputState& input, CursorMode mode);
}
