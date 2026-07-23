#include "tbx/platform/input.h"
#include "tbx/debug/log.h"
#include "tbx/events/events.h"
#include "tbx/platform/keys.h"
#include <SDL3/SDL.h>
#include <array>

namespace tbx
{
    // Open controllers live behind this backend seam; SDL types never escape the sdl/ folder.
    // These are the sanctioned mutable process statics (mirroring window_sdl.cpp's GL context):
    // a slot table pairing each open gamepad's SDL instance id with its handle. A zero id marks
    // a free slot — SDL never issues 0 as a valid SDL_JoystickID. Reclaimed by SDL_Quit() at the
    // last window's teardown (window_sdl.cpp), after which they are inert (no further pump runs).
    static std::array<SDL_JoystickID, MAX_GAMEPADS> g_gamepad_ids = {};
    static std::array<SDL_Gamepad*, MAX_GAMEPADS> g_gamepad_handles = {};
    static bool g_gamepad_subsystem_ready = false;

    //// STATE FEED ////
    // Writing InputState is the backend's own business — there is no public feed API. These roll
    // the frame and record transitions straight into the plain-data state during update_input.

    static bool is_valid_gamepad(const int slot)
    {
        return slot >= 0 && slot < MAX_GAMEPADS;
    }

    static void roll_input_frame(InputState& input)
    {
        input.previous_keys = input.keys;
        input.previous_mouse = input.mouse;
        input.mouse_delta = Vec2(0.0f, 0.0f);
        input.scroll_delta = 0.0f;
        // Buttons roll for edge queries; axes are absolute levels, so they persist untouched.
        for (GamepadState& gamepad : input.gamepads)
            gamepad.previous_buttons = gamepad.buttons;
    }

    static void feed_key(InputState& input, const Key key, const bool is_down)
    {
        input.keys[static_cast<size>(key)] = is_down;
    }

    static void feed_mouse_button(InputState& input, const MouseButton button, const bool is_down)
    {
        input.mouse[static_cast<size>(button)] = is_down;
    }

    static void feed_mouse_move(InputState& input, const Vec2 position, const Vec2 delta)
    {
        input.mouse_position = position;
        input.mouse_delta += delta;
    }

    static void feed_scroll(InputState& input, const float delta)
    {
        input.scroll_delta += delta;
    }

    static void feed_gamepad_connected(InputState& input, const int slot, const bool is_connected)
    {
        if (!is_valid_gamepad(slot))
            return;
        GamepadState& gamepad = input.gamepads[static_cast<size>(slot)];
        gamepad.is_connected = is_connected;
        if (!is_connected)
        {
            // A vacated slot must not report stale held buttons or off-center sticks.
            gamepad.buttons = {};
            gamepad.previous_buttons = {};
            gamepad.axes = {};
        }
    }

    static void feed_gamepad_button(
        InputState& input,
        const int slot,
        const GamepadButton button,
        const bool is_down)
    {
        if (!is_valid_gamepad(slot))
            return;
        input.gamepads[static_cast<size>(slot)].buttons[static_cast<size>(button)] = is_down;
    }

    static void feed_gamepad_axis(
        InputState& input,
        const int slot,
        const GamepadAxis axis,
        const float value)
    {
        if (!is_valid_gamepad(slot))
            return;
        input.gamepads[static_cast<size>(slot)].axes[static_cast<size>(axis)] = value;
    }

    //// TRANSLATION ////

    static Key translate_key(const SDL_Scancode scancode)
    {
        // Contiguous SDL ranges map onto contiguous Key ranges.
        if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
            return static_cast<Key>(static_cast<int>(Key::A) + (scancode - SDL_SCANCODE_A));
        if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
            return static_cast<Key>(static_cast<int>(Key::NUM_1) + (scancode - SDL_SCANCODE_1));
        if (scancode == SDL_SCANCODE_0)
            return Key::NUM_0;
        if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
            return static_cast<Key>(static_cast<int>(Key::F1) + (scancode - SDL_SCANCODE_F1));

        switch (scancode)
        {
            case SDL_SCANCODE_ESCAPE:
                return Key::ESCAPE;
            case SDL_SCANCODE_TAB:
                return Key::TAB;
            case SDL_SCANCODE_CAPSLOCK:
                return Key::CAPS_LOCK;
            case SDL_SCANCODE_SPACE:
                return Key::SPACE;
            case SDL_SCANCODE_RETURN:
                return Key::ENTER;
            case SDL_SCANCODE_BACKSPACE:
                return Key::BACKSPACE;
            case SDL_SCANCODE_DELETE:
                return Key::DEL;
            case SDL_SCANCODE_INSERT:
                return Key::INSERT;
            case SDL_SCANCODE_HOME:
                return Key::HOME;
            case SDL_SCANCODE_END:
                return Key::END;
            case SDL_SCANCODE_PAGEUP:
                return Key::PAGE_UP;
            case SDL_SCANCODE_PAGEDOWN:
                return Key::PAGE_DOWN;
            case SDL_SCANCODE_LEFT:
                return Key::LEFT;
            case SDL_SCANCODE_RIGHT:
                return Key::RIGHT;
            case SDL_SCANCODE_UP:
                return Key::UP;
            case SDL_SCANCODE_DOWN:
                return Key::DOWN;
            case SDL_SCANCODE_LSHIFT:
                return Key::LEFT_SHIFT;
            case SDL_SCANCODE_RSHIFT:
                return Key::RIGHT_SHIFT;
            case SDL_SCANCODE_LCTRL:
                return Key::LEFT_CTRL;
            case SDL_SCANCODE_RCTRL:
                return Key::RIGHT_CTRL;
            case SDL_SCANCODE_LALT:
                return Key::LEFT_ALT;
            case SDL_SCANCODE_RALT:
                return Key::RIGHT_ALT;
            case SDL_SCANCODE_MINUS:
                return Key::MINUS;
            case SDL_SCANCODE_EQUALS:
                return Key::EQUALS;
            case SDL_SCANCODE_LEFTBRACKET:
                return Key::LEFT_BRACKET;
            case SDL_SCANCODE_RIGHTBRACKET:
                return Key::RIGHT_BRACKET;
            case SDL_SCANCODE_BACKSLASH:
                return Key::BACKSLASH;
            case SDL_SCANCODE_SEMICOLON:
                return Key::SEMICOLON;
            case SDL_SCANCODE_APOSTROPHE:
                return Key::APOSTROPHE;
            case SDL_SCANCODE_GRAVE:
                return Key::GRAVE;
            case SDL_SCANCODE_COMMA:
                return Key::COMMA;
            case SDL_SCANCODE_PERIOD:
                return Key::PERIOD;
            case SDL_SCANCODE_SLASH:
                return Key::SLASH;
            default:
                return Key::UNKNOWN;
        }
    }

    static MouseButton translate_mouse_button(const Uint8 button)
    {
        switch (button)
        {
            case SDL_BUTTON_LEFT:
                return MouseButton::LEFT;
            case SDL_BUTTON_RIGHT:
                return MouseButton::RIGHT;
            case SDL_BUTTON_MIDDLE:
                return MouseButton::MIDDLE;
            default:
                return MouseButton::COUNT;
        }
    }

    static GamepadButton translate_gamepad_button(const SDL_GamepadButton button)
    {
        // Our GamepadButton enum mirrors SDL's ordering through DPAD_RIGHT; the tail SDL adds
        // (misc buttons, paddles, touchpad) has no engine identity and maps to COUNT.
        if (button >= SDL_GAMEPAD_BUTTON_SOUTH && button <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
            return static_cast<GamepadButton>(button);
        return GamepadButton::COUNT;
    }

    static GamepadAxis translate_gamepad_axis(const SDL_GamepadAxis axis)
    {
        // Contiguous with our GamepadAxis enum (sticks then triggers).
        if (axis >= SDL_GAMEPAD_AXIS_LEFTX && axis <= SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            return static_cast<GamepadAxis>(axis);
        return GamepadAxis::COUNT;
    }

    //// GAMEPAD SLOTS ////

    static int find_gamepad_slot(const SDL_JoystickID id)
    {
        for (int slot = 0; slot < MAX_GAMEPADS; ++slot)
            if (g_gamepad_ids[static_cast<size>(slot)] == id)
                return slot;
        return -1;
    }

    static void open_gamepad(InputState& input, EventsState& events, const SDL_JoystickID id)
    {
        if (find_gamepad_slot(id) >= 0)
            return; // already tracked (startup enumeration and the ADDED event can both hit)
        const int slot = find_gamepad_slot(0); // first free slot (0 == empty)
        if (slot < 0)
        {
            TBX_WARN("gamepad ignored: all {} controller slots are in use", MAX_GAMEPADS);
            return;
        }
        SDL_Gamepad* handle = SDL_OpenGamepad(id);
        if (!handle)
        {
            TBX_WARN("SDL_OpenGamepad failed: {}", SDL_GetError());
            return;
        }
        g_gamepad_ids[static_cast<size>(slot)] = id;
        g_gamepad_handles[static_cast<size>(slot)] = handle;
        feed_gamepad_connected(input, slot, true);
        TBX_INFO("controller connected: '{}' -> slot {}", SDL_GetGamepadName(handle), slot);
        events.signal<InputDeviceConnected>().emit({.index = slot});
    }

    static void close_gamepad(InputState& input, EventsState& events, const SDL_JoystickID id)
    {
        const int slot = find_gamepad_slot(id);
        if (slot < 0)
            return;
        if (g_gamepad_handles[static_cast<size>(slot)])
            SDL_CloseGamepad(g_gamepad_handles[static_cast<size>(slot)]);
        g_gamepad_ids[static_cast<size>(slot)] = 0;
        g_gamepad_handles[static_cast<size>(slot)] = nullptr;
        feed_gamepad_connected(input, slot, false);
        TBX_INFO("controller disconnected: slot {}", slot);
        events.signal<InputDeviceDisconnected>().emit({.index = slot});
    }

    static void feed_gamepad_event(InputState& input, EventsState& events, const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_EVENT_GAMEPAD_ADDED:
                open_gamepad(input, events, event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                close_gamepad(input, events, event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
            {
                const int slot = find_gamepad_slot(event.gbutton.which);
                const GamepadButton button =
                    translate_gamepad_button(static_cast<SDL_GamepadButton>(event.gbutton.button));
                if (slot >= 0 && button != GamepadButton::COUNT)
                    feed_gamepad_button(input, slot, button, event.gbutton.down);
                break;
            }
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            {
                const int slot = find_gamepad_slot(event.gaxis.which);
                const GamepadAxis axis =
                    translate_gamepad_axis(static_cast<SDL_GamepadAxis>(event.gaxis.axis));
                if (slot >= 0 && axis != GamepadAxis::COUNT)
                {
                    // Sint16 [-32768, 32767] -> [-1, 1]; the negative extreme divides past -1,
                    // so clamp. Triggers arrive in [0, 32767] and land in [0, 1].
                    const float value = SDL_clamp(event.gaxis.value / 32767.0f, -1.0f, 1.0f);
                    feed_gamepad_axis(input, slot, axis, value);
                }
                break;
            }
            default:
                break;
        }
    }

    //// INPUT ////

    void internal::update_input(InputState& input, EventsState& events)
    {
        // The frame roll is backend-agnostic and must run even headless: gameplay's edge queries
        // (is_pressed/is_released) depend on previous-state rolling regardless of any window.
        roll_input_frame(input);

        // No window means SDL was never initialized (headless tests/tooling): nothing to pump.
        if (!SDL_WasInit(SDL_INIT_VIDEO))
            return;

        // Controllers init lazily the first windowed frame. Enumerate the pads already plugged
        // in right here rather than trusting the ADDED events alone — open_gamepad de-dupes, so
        // a pad reported by both paths is claimed once.
        if (!g_gamepad_subsystem_ready)
        {
            if (SDL_InitSubSystem(SDL_INIT_GAMEPAD))
            {
                g_gamepad_subsystem_ready = true;
                int gamepad_count = 0;
                if (SDL_JoystickID* ids = SDL_GetGamepads(&gamepad_count))
                {
                    for (int i = 0; i < gamepad_count; ++i)
                        open_gamepad(input, events, ids[i]);
                    SDL_free(ids);
                }
            }
            else
                TBX_WARN("SDL_INIT_GAMEPAD failed: {}", SDL_GetError());
        }

        // The full catch-all drain: update_windows() ran first and already took the window/quit
        // band, so what remains is input (keyboard/mouse/controller) plus stray families SDL
        // posts unsolicited (clipboard, display) — the default case discards those so the queue
        // never grows.
        auto event = SDL_Event {};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                {
                    const Key key = translate_key(event.key.scancode);
                    if (key == Key::UNKNOWN)
                        break;
                    if (!event.key.repeat)
                        feed_key(input, key, event.key.down);
                    events.signal<InputEvent>().emit(
                        {.key = key, .is_down = event.key.down, .is_repeat = event.key.repeat != 0});
                    break;
                }
                case SDL_EVENT_MOUSE_MOTION:
                    feed_mouse_move(
                        input,
                        Vec2(event.motion.x, event.motion.y),
                        Vec2(event.motion.xrel, event.motion.yrel));
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                {
                    const MouseButton button = translate_mouse_button(event.button.button);
                    if (button != MouseButton::COUNT)
                        feed_mouse_button(input, button, event.button.down);
                    break;
                }
                case SDL_EVENT_MOUSE_WHEEL:
                    feed_scroll(input, event.wheel.y);
                    break;
                case SDL_EVENT_GAMEPAD_ADDED:
                case SDL_EVENT_GAMEPAD_REMOVED:
                case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                case SDL_EVENT_GAMEPAD_BUTTON_UP:
                case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                    feed_gamepad_event(input, events, event);
                    break;
                default:
                    break;
            }
        }
    }
}
