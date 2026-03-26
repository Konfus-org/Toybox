#include "tbx/plugins/sdl_input/sdl_input_plugin.h"
#include "tbx/debugging/macros.h"
#include <array>
#include <memory>
#include <utility>

namespace sdl_input
{
    namespace
    {
        constexpr Uint32 GamepadSubsystemMask = SDL_INIT_GAMEPAD;

        template <typename TRequest, typename TResult>
        void complete_success(TRequest& request, TResult&& result)
        {
            request.result = std::forward<TResult>(result);
            request.state = tbx::MessageState::HANDLED;
            request.Message::result.flag_success();
        }

        void set_error_report(std::string* out_error_report, const std::string& message)
        {
            if (out_error_report)
                *out_error_report = message;
        }
    }

    void SdlInputPlugin::on_attach(tbx::IPluginHost&)
    {
        if ((SDL_WasInit(GamepadSubsystemMask) & GamepadSubsystemMask) == GamepadSubsystemMask)
        {
            _owns_gamepad_subsystem = false;
            SDL_AddEventWatch(accumulate_wheel_delta, this);
            return;
        }

        if (!SDL_InitSubSystem(GamepadSubsystemMask))
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
        _requested_mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;

        if (_owns_gamepad_subsystem)
            SDL_QuitSubSystem(GamepadSubsystemMask);
        _owns_gamepad_subsystem = false;
    }

    void SdlInputPlugin::on_update(const tbx::DeltaTime&)
    {
        _wheel_delta = 0.0F;
        apply_mouse_lock_mode();
    }

    void SdlInputPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (auto* request = handle_message<tbx::KeyboardStateRequest>(msg))
        {
            handle_keyboard_request(*request);
            return;
        }

        if (auto* request = handle_message<tbx::MouseStateRequest>(msg))
        {
            handle_mouse_request(*request);
            return;
        }

        if (auto* request = handle_message<tbx::SetMouseLockRequest>(msg))
        {
            handle_set_mouse_lock_request(*request);
            return;
        }

        if (auto* request = handle_message<tbx::MouseLockModeRequest>(msg))
        {
            handle_mouse_lock_mode_request(*request);
            return;
        }

        if (auto* request = handle_message<tbx::ControllerStateRequest>(msg))
        {
            handle_controller_request(*request);
            return;
        }
    }

    void SdlInputPlugin::handle_keyboard_request(tbx::KeyboardStateRequest& request)
    {
        tbx::KeyboardState state = {};

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

        complete_success(request, std::move(state));
    }

    void SdlInputPlugin::handle_mouse_request(tbx::MouseStateRequest& request) const
    {
        tbx::MouseState state = {};

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
            if ((buttons & button_mask) != 0)
                state.pressed_buttons.insert(button_id);

        complete_success(request, std::move(state));
    }

    void SdlInputPlugin::handle_set_mouse_lock_request(tbx::SetMouseLockRequest& request)
    {
        _requested_mouse_lock_mode = request.mode;
        request.state = tbx::MessageState::HANDLED;
        request.Message::result.flag_success();
    }

    void SdlInputPlugin::handle_mouse_lock_mode_request(tbx::MouseLockModeRequest& request) const
    {
        request.result = _mouse_lock_mode;
        request.state = tbx::MessageState::HANDLED;
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
        SDL_Window* target_window = SDL_GetMouseFocus();
        if (!target_window)
            target_window = SDL_GetKeyboardFocus();

        if (_requested_mouse_lock_mode == tbx::MouseLockMode::UNLOCKED)
        {
            const bool released = release_mouse_lock_window(out_error_report);
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            return released;
        }

        tbx::MouseLockMode target_mode = _requested_mouse_lock_mode;
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
            set_error_report(out_error_report, "No focused window is available for mouse lock mode.");
            return false;
        }

        if (target_mode == tbx::MouseLockMode::RELATIVE)
        {
            if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
            {
                _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
                set_error_report(
                    out_error_report,
                    std::string("Failed to disable mouse grab before relative mode: ")
                    + SDL_GetError());
                return false;
            }

            if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, true))
            {
                _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
                set_error_report(
                    out_error_report,
                    std::string("Failed to enable relative mouse mode: ") + SDL_GetError());
                return false;
            }

            _mouse_lock_mode = tbx::MouseLockMode::RELATIVE;
            return true;
        }

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            set_error_report(
                out_error_report,
                std::string("Failed to disable relative mouse mode before grab mode: ")
                + SDL_GetError());
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, true))
        {
            _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
            set_error_report(
                out_error_report,
                std::string("Failed to enable mouse grab mode: ") + SDL_GetError());
            return false;
        }

        _mouse_lock_mode = tbx::MouseLockMode::INPUT_GRABBED;
        return true;
    }

    bool SdlInputPlugin::release_mouse_lock_window(std::string* out_error_report)
    {
        if (!_mouse_lock_window)
            return true;

        if (!SDL_SetWindowRelativeMouseMode(_mouse_lock_window, false))
        {
            set_error_report(
                out_error_report,
                std::string("Failed to disable relative mouse mode: ") + SDL_GetError());
            return false;
        }

        if (!SDL_SetWindowMouseGrab(_mouse_lock_window, false))
        {
            set_error_report(
                out_error_report,
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

    void SdlInputPlugin::handle_controller_request(tbx::ControllerStateRequest& request)
    {
        tbx::ControllerState state = {};
        state.controller_index = request.controller_index;

        int gamepad_count = 0;
        auto gamepad_ids = std::unique_ptr<SDL_JoystickID, decltype(&SDL_free)>(
            SDL_GetGamepads(&gamepad_count),
            SDL_free);

        if (!gamepad_ids || request.controller_index < 0
            || request.controller_index >= gamepad_count)
        {
            complete_success(request, std::move(state));
            return;
        }

        auto gamepad = std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>(
            SDL_OpenGamepad(gamepad_ids.get()[request.controller_index]),
            SDL_CloseGamepad);

        if (!gamepad)
        {
            complete_success(request, std::move(state));
            return;
        }

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

        complete_success(request, std::move(state));
    }
}
