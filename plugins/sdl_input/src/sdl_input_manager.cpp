#include "sdl_input_manager.h"
#include <array>
#include <memory>
#include <utility>

namespace sdl_input
{
    SdlInputManager::~SdlInputManager() noexcept
    {
        std::string error_report = {};
        release_mouse_lock_window(&error_report);
        _requested_mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
    }

    void SdlInputManager::add_wheel_delta(float wheel_delta)
    {
        _wheel_delta += wheel_delta;
    }

    tbx::KeyboardState SdlInputManager::get_keyboard_state() const
    {
        auto state = tbx::KeyboardState {};

        int key_count = 0;
        const bool* key_states = SDL_GetKeyboardState(&key_count);
        if (!key_states || key_count <= 0)
            return state;

        for (int key_index = 0; key_index < key_count; ++key_index)
        {
            if (key_states[key_index])
                state.pressed_keys.insert(key_index);
        }

        return state;
    }

    tbx::ControllerState SdlInputManager::get_controller_state(int controller_index) const
    {
        auto state = tbx::ControllerState {};
        state.controller_index = controller_index;

        int gamepad_count = 0;
        auto gamepad_ids = std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)>(
            SDL_GetGamepads(&gamepad_count),
            SDL_free);

        if (!gamepad_ids || controller_index < 0 || controller_index >= gamepad_count)
            return state;

        auto gamepad = std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>(
            SDL_OpenGamepad(gamepad_ids.get()[controller_index]),
            SDL_CloseGamepad);

        if (!gamepad)
            return state;

        state.is_connected = true;

        for (int button = SDL_GAMEPAD_BUTTON_SOUTH; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
        {
            if (SDL_GetGamepadButton(gamepad.get(), static_cast<SDL_GamepadButton>(button)))
                state.pressed_buttons.insert(button);
        }

        for (int axis = SDL_GAMEPAD_AXIS_LEFTX; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis)
        {
            const Sint16 axis_value =
                SDL_GetGamepadAxis(gamepad.get(), static_cast<SDL_GamepadAxis>(axis));
            state.axis_values.emplace(axis, static_cast<float>(axis_value) / 32767.0F);
        }

        return state;
    }

    tbx::MouseState SdlInputManager::get_mouse_state() const
    {
        auto state = tbx::MouseState {};

        float x = 0.0F;
        float y = 0.0F;
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
        state.position = tbx::Vec2(x, y);

        float delta_x = 0.0F;
        float delta_y = 0.0F;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        state.delta = tbx::Vec2(delta_x, delta_y);
        state.wheel_delta = _wheel_delta;

        constexpr auto mouse_buttons = std::array<std::pair<SDL_MouseButtonFlags, int>, 5> {
            std::pair(SDL_BUTTON_LMASK, 1),
            std::pair(SDL_BUTTON_MMASK, 2),
            std::pair(SDL_BUTTON_RMASK, 3),
            std::pair(SDL_BUTTON_X1MASK, 4),
            std::pair(SDL_BUTTON_X2MASK, 5)};

        for (const auto& [button_mask, button_id] : mouse_buttons)
        {
            if ((buttons & button_mask) != 0)
                state.pressed_buttons.insert(button_id);
        }

        return state;
    }

    void SdlInputManager::set_mouse_lock_mode(tbx::MouseLockMode mode)
    {
        _requested_mouse_lock_mode = mode;
    }

    tbx::MouseLockMode SdlInputManager::get_mouse_lock_mode() const
    {
        return _mouse_lock_mode;
    }

    void SdlInputManager::update_backend_state()
    {
        _wheel_delta = 0.0F;
        apply_mouse_lock_mode();
    }

    bool SdlInputManager::apply_mouse_lock_mode(std::string* out_error_report)
    {
        SDL_Window* target_window = SDL_GetMouseFocus();
        if (!target_window)
            target_window = SDL_GetKeyboardFocus();

        if (_requested_mouse_lock_mode == tbx::MouseLockMode::UNLOCKED)
        {
            const bool released = release_mouse_lock_window(out_error_report);
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            return released;
        }

        auto target_mode = _requested_mouse_lock_mode;
        if (is_maximized_fullscreen_window(target_window))
            target_mode = tbx::MouseLockMode::INPUT_GRABBED;

        if (_mouse_lock_window && _mouse_lock_window != target_window)
        {
            if (!release_mouse_lock_window(out_error_report))
            {
                _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
                return false;
            }
        }

        _mouse_lock_window = target_window;
        if (!_mouse_lock_window)
        {
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            if (out_error_report)
                *out_error_report = "No focused window is available for mouse lock mode.";
            return false;
        }

        if (target_mode == tbx::MouseLockMode::RELATIVE)
        {
            if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
            {
                _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
                if (out_error_report)
                {
                    *out_error_report =
                        std::string("Failed to disable mouse grab before relative mode: ")
                        + SDL_GetError();
                }
                return false;
            }

            if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, true))
            {
                _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
                if (out_error_report)
                {
                    *out_error_report =
                        std::string("Failed to enable relative mouse mode: ") + SDL_GetError();
                }
                return false;
            }

            _mouse_lock_mode = tbx::MouseLockMode::RELATIVE;
            return true;
        }

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            if (out_error_report)
            {
                *out_error_report =
                    std::string("Failed to disable relative mouse mode before grab mode: ")
                    + SDL_GetError();
            }
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, true))
        {
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            if (out_error_report)
                *out_error_report =
                    std::string("Failed to enable mouse grab mode: ") + SDL_GetError();
            return false;
        }

        _mouse_lock_mode = tbx::MouseLockMode::INPUT_GRABBED;
        return true;
    }

    bool SdlInputManager::release_mouse_lock_window(std::string* out_error_report)
    {
        if (!_mouse_lock_window)
            return true;

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            if (out_error_report)
                *out_error_report =
                    std::string("Failed to disable relative mouse mode: ") + SDL_GetError();
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
        {
            if (out_error_report)
                *out_error_report =
                    std::string("Failed to disable mouse grab mode: ") + SDL_GetError();
            return false;
        }

        _mouse_lock_window = nullptr;
        return true;
    }

    bool SdlInputManager::is_maximized_fullscreen_window(SDL_Window* window)
    {
        if (!window)
            return false;

        const SDL_WindowFlags window_flags = SDL_GetWindowFlags(window);
        const bool is_fullscreen = (window_flags & SDL_WINDOW_FULLSCREEN) != 0;
        const bool is_maximized = (window_flags & SDL_WINDOW_MAXIMIZED) != 0;
        return is_fullscreen && is_maximized;
    }
}
