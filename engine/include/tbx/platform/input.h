#pragma once
#include "tbx/api.h"
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
    /// Purpose: The cursor mode gameplay asked for (the platform backend applies it during
    /// the pump; headless windows ignore it).
    TBX_API CursorMode get_cursor_mode(const InputState& input);

    /// @brief
    /// Purpose: Asks for a cursor mode — NORMAL frees the pointer, LOCKED grabs it for
    /// mouse-look (deltas keep flowing). Takes effect at the next pump.
    TBX_API void set_cursor_mode(InputState& input, CursorMode mode);

    // Button queries — one overloaded verb per edge, across keys, mouse, and gamepads. The
    // gamepad `slot` defaults to 0 (the first controller); out-of-range slots read false.

    /// @brief
    /// Purpose: True while a controller is plugged into this slot. Out-of-range slots read false.
    TBX_API bool is_gamepad_connected(const InputState& input, int slot);

    /// @brief
    /// Purpose: True while the key / mouse button / gamepad button is held.
    TBX_API bool is_down(const InputState& input, Key key);
    TBX_API bool is_down(const InputState& input, MouseButton button);
    TBX_API bool is_down(const InputState& input, GamepadButton button, int slot = 0);

    /// @brief
    /// Purpose: True only on the frame the key / mouse button / gamepad button went down.
    TBX_API bool is_pressed(const InputState& input, Key key);
    TBX_API bool is_pressed(const InputState& input, MouseButton button);
    TBX_API bool is_pressed(const InputState& input, GamepadButton button, int slot = 0);

    /// @brief
    /// Purpose: True only on the frame the key / mouse button / gamepad button went up.
    TBX_API bool is_released(const InputState& input, Key key);
    TBX_API bool is_released(const InputState& input, MouseButton button);
    TBX_API bool is_released(const InputState& input, GamepadButton button, int slot = 0);

    /// @brief
    /// Purpose: An analog axis level. Gamepad sticks read [-1, 1], triggers [0, 1] (out-of-range
    /// slots read 0); mouse X/Y are the pointer position in window pixels (SCROLL reads 0 here —
    /// it is delta-only). Callers apply their own deadzone.
    TBX_API float get_axis(const InputState& input, GamepadAxis axis, int slot = 0);
    TBX_API float get_axis(const InputState& input, MouseAxis axis);

    /// @brief
    /// Purpose: This frame's change in a mouse axis — X/Y are pointer movement, SCROLL is wheel
    /// travel. (Gamepad axes are absolute levels, so they have no delta.)
    TBX_API float get_axis_delta(const InputState& input, MouseAxis axis);

    /// @brief
    /// Purpose: The per-frame input pass: rolls frame state, then pumps OS input events
    /// (keyboard, mouse, controllers) into the input state, emitting key transitions through
    /// events. Implemented by the platform backend; headless runs roll but skip the pump.
    TBX_API void update_input(InputState& input, EventsState& events);
}
