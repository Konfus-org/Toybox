#include "sdl_input_plugin.h"
#include "tbx/debugging/macros.h"

namespace sdl_input
{
    using namespace tbx;
    void SdlInputPlugin::on_attach(IPluginHost&)
    {
        const Uint32 mask = SDL_INIT_GAMEPAD;
        if ((SDL_WasInit(mask) & mask) == mask)
        {
            _owns_gamepad_subsystem = false;
            SDL_AddEventWatch(accumulate_wheel_delta, this);
            return;
        }

        if (!SDL_InitSubSystem(mask))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL gamepad subsystem.");
            _owns_gamepad_subsystem = false;
            return;
        }

        SDL_AddEventWatch(accumulate_wheel_delta, this);

        _owns_gamepad_subsystem = true;
    }

    void SdlInputPlugin::on_detach()
    {
        SDL_RemoveEventWatch(accumulate_wheel_delta, this);
        std::string error_report = {};
        release_mouse_lock_window(&error_report);
        _requested_mouse_lock_mode = MouseLockMode::UNLOCKED;
        _mouse_lock_mode = MouseLockMode::UNLOCKED;

        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        _owns_gamepad_subsystem = false;
    }

    void SdlInputPlugin::on_update(const DeltaTime&)
    {
        _wheel_delta = 0.0F;
        apply_mouse_lock_mode();
    }

    void SdlInputPlugin::on_recieve_message(Message& msg)
    {
        if (auto* request = handle_message<KeyboardStateRequest>(msg))
        {
            handle_keyboard_request(*request);
            return;
        }

        if (auto* request = handle_message<MouseStateRequest>(msg))
        {
            handle_mouse_request(*request);
            return;
        }

        if (auto* request = handle_message<SetMouseLockRequest>(msg))
        {
            handle_set_mouse_lock_request(*request);
            return;
        }

        if (auto* request = handle_message<MouseLockModeRequest>(msg))
        {
            handle_mouse_lock_mode_request(*request);
            return;
        }

        if (auto* request = handle_message<ControllerStateRequest>(msg))
        {
            handle_controller_request(*request);
            return;
        }
    }

    void SdlInputPlugin::handle_keyboard_request(KeyboardStateRequest& request) const
    {
        KeyboardState state = {};

        int key_count = 0;
        const bool* key_states = SDL_GetKeyboardState(&key_count);
        if (key_states && key_count > 0)
        {
            for (int key_index = 0; key_index < key_count; ++key_index)
            {
                if (key_states[key_index])
                    state.pressed_keys.insert(key_index);
            }
        }

        request.result = std::move(state);
        request.state = MessageState::HANDLED;
        request.Message::result.flag_success();
    }

    void SdlInputPlugin::handle_mouse_request(MouseStateRequest& request)
    {
        MouseState state = {};

        float x = 0.0F;
        float y = 0.0F;
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
        state.position = Vec2(x, y);

        float delta_x = 0.0F;
        float delta_y = 0.0F;
        SDL_GetRelativeMouseState(&delta_x, &delta_y);
        state.delta = Vec2(delta_x, delta_y);
        state.wheel_delta = _wheel_delta;

        constexpr int LEFT_BUTTON = 1;
        constexpr int MIDDLE_BUTTON = 2;
        constexpr int RIGHT_BUTTON = 3;
        constexpr int X1_BUTTON = 4;
        constexpr int X2_BUTTON = 5;

        if ((buttons & SDL_BUTTON_LMASK) != 0)
            state.pressed_buttons.insert(LEFT_BUTTON);
        if ((buttons & SDL_BUTTON_MMASK) != 0)
            state.pressed_buttons.insert(MIDDLE_BUTTON);
        if ((buttons & SDL_BUTTON_RMASK) != 0)
            state.pressed_buttons.insert(RIGHT_BUTTON);
        if ((buttons & SDL_BUTTON_X1MASK) != 0)
            state.pressed_buttons.insert(X1_BUTTON);
        if ((buttons & SDL_BUTTON_X2MASK) != 0)
            state.pressed_buttons.insert(X2_BUTTON);

        request.result = std::move(state);
        request.state = MessageState::HANDLED;
        request.Message::result.flag_success();
    }

    void SdlInputPlugin::handle_set_mouse_lock_request(SetMouseLockRequest& request)
    {
        _requested_mouse_lock_mode = request.mode;
        std::string error_report = {};
        const bool succeeded = apply_mouse_lock_mode(&error_report);
        request.state = MessageState::HANDLED;
        if (succeeded)
            request.Message::result.flag_success();
        else
            request.Message::result.flag_failure(error_report);
    }

    void SdlInputPlugin::handle_mouse_lock_mode_request(MouseLockModeRequest& request) const
    {
        request.result = _mouse_lock_mode;
        request.state = MessageState::HANDLED;
        request.Message::result.flag_success();
    }

    bool SdlInputPlugin::accumulate_wheel_delta(void* userdata, SDL_Event* event)
    {
        if (!userdata || !event)
            return true;

        if (event->type != SDL_EVENT_MOUSE_WHEEL)
            return true;

        auto* plugin = static_cast<SdlInputPlugin*>(userdata);
        plugin->_wheel_delta += event->wheel.y;
        return true;
    }

    bool SdlInputPlugin::apply_mouse_lock_mode(std::string* out_error_report)
    {
        auto assign_error_report = [out_error_report](const std::string& message)
        {
            if (out_error_report)
                *out_error_report = message;
        };

        SDL_Window* target_window = SDL_GetMouseFocus();
        if (!target_window)
            target_window = SDL_GetKeyboardFocus();

        if (_requested_mouse_lock_mode == MouseLockMode::UNLOCKED)
        {
            const bool released = release_mouse_lock_window(out_error_report);
            _mouse_lock_mode = MouseLockMode::UNLOCKED;
            return released;
        }

        MouseLockMode target_mode = _requested_mouse_lock_mode;
        if (target_mode != MouseLockMode::UNLOCKED && is_maximized_fullscreen_window(target_window))
            target_mode = MouseLockMode::INPUT_GRABBED;

        if (_mouse_lock_window && _mouse_lock_window != target_window)
        {
            if (!release_mouse_lock_window(out_error_report))
            {
                _mouse_lock_mode = MouseLockMode::UNLOCKED;
                return false;
            }
        }

        _mouse_lock_window = target_window;

        if (!_mouse_lock_window)
        {
            _mouse_lock_mode = MouseLockMode::UNLOCKED;
            assign_error_report("No focused window is available for mouse lock mode.");
            return false;
        }

        if (target_mode == MouseLockMode::RELATIVE)
        {
            if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
            {
                _mouse_lock_mode = MouseLockMode::UNLOCKED;
                assign_error_report(
                    std::string("Failed to disable mouse grab before relative mode: ")
                    + SDL_GetError());
                return false;
            }

            if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, true))
            {
                _mouse_lock_mode = MouseLockMode::UNLOCKED;
                assign_error_report(
                    std::string("Failed to enable relative mouse mode: ") + SDL_GetError());
                return false;
            }

            _mouse_lock_mode = MouseLockMode::RELATIVE;
            return true;
        }

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            _mouse_lock_mode = MouseLockMode::UNLOCKED;
            assign_error_report(
                std::string("Failed to disable relative mouse mode before grab mode: ")
                + SDL_GetError());
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, true))
        {
            _mouse_lock_mode = MouseLockMode::UNLOCKED;
            assign_error_report(std::string("Failed to enable mouse grab mode: ") + SDL_GetError());
            return false;
        }

        _mouse_lock_mode = MouseLockMode::INPUT_GRABBED;
        return true;
    }

    bool SdlInputPlugin::release_mouse_lock_window(std::string* out_error_report)
    {
        auto assign_error_report = [out_error_report](const std::string& message)
        {
            if (out_error_report)
                *out_error_report = message;
        };

        if (!_mouse_lock_window)
            return true;

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            assign_error_report(
                std::string("Failed to disable relative mouse mode: ") + SDL_GetError());
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
        {
            assign_error_report(
                std::string("Failed to disable mouse grab mode: ") + SDL_GetError());
            return false;
        }

        _mouse_lock_window = nullptr;
        return true;
    }

    bool SdlInputPlugin::is_maximized_fullscreen_window(SDL_Window* window)
    {
        if (!window)
            return false;

        const SDL_WindowFlags window_flags = SDL_GetWindowFlags(window);
        const bool is_fullscreen = (window_flags & SDL_WINDOW_FULLSCREEN) != 0;
        const bool is_maximized = (window_flags & SDL_WINDOW_MAXIMIZED) != 0;
        return is_fullscreen && is_maximized;
    }

    void SdlInputPlugin::handle_controller_request(ControllerStateRequest& request) const
    {
        ControllerState state = {};
        state.controller_index = request.controller_index;

        int gamepad_count = 0;
        SDL_JoystickID* gamepad_ids = SDL_GetGamepads(&gamepad_count);
        if (!gamepad_ids || request.controller_index < 0
            || request.controller_index >= gamepad_count)
        {
            if (gamepad_ids)
                SDL_free(gamepad_ids);

            request.result = std::move(state);
            request.state = MessageState::HANDLED;
            request.Message::result.flag_success();
            return;
        }

        SDL_Gamepad* gamepad = SDL_OpenGamepad(gamepad_ids[request.controller_index]);
        if (!gamepad)
        {
            SDL_free(gamepad_ids);
            request.result = std::move(state);
            request.state = MessageState::HANDLED;
            request.Message::result.flag_success();
            return;
        }

        state.is_connected = true;

        for (int button = SDL_GAMEPAD_BUTTON_SOUTH; button < SDL_GAMEPAD_BUTTON_COUNT; ++button)
        {
            if (SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(button)))
                state.pressed_buttons.insert(button);
        }

        for (int axis = SDL_GAMEPAD_AXIS_LEFTX; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis)
        {
            const Sint16 axis_value =
                SDL_GetGamepadAxis(gamepad, static_cast<SDL_GamepadAxis>(axis));
            state.axis_values.emplace(axis, static_cast<float>(axis_value) / 32767.0F);
        }

        SDL_CloseGamepad(gamepad);
        SDL_free(gamepad_ids);

        request.result = std::move(state);
        request.state = MessageState::HANDLED;
        request.Message::result.flag_success();
    }
}
