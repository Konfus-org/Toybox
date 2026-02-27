#pragma once
#include "tbx/input/input_messages.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>
#include <string>

namespace sdl_input
{
    using namespace tbx;
    /// <summary>Provides SDL-backed keyboard, mouse, and controller state requests.</summary>
    /// <remarks>Purpose: Handles core input requests and translates SDL state into engine messages.
    /// Ownership: Owns initialization state for the SDL gamepad subsystem.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.</remarks>
    class SdlInputPlugin final : public Plugin
    {
      public:
        void on_attach(IPluginHost& host) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_recieve_message(Message& msg) override;

      private:
        void handle_keyboard_request(KeyboardStateRequest& request) const;
        void handle_mouse_request(MouseStateRequest& request);
        void handle_set_mouse_lock_request(SetMouseLockRequest& request);
        void handle_mouse_lock_mode_request(MouseLockModeRequest& request) const;
        void handle_controller_request(ControllerStateRequest& request) const;
        bool apply_mouse_lock_mode(std::string* out_error_report = nullptr);
        bool release_mouse_lock_window(std::string* out_error_report = nullptr);
        static bool is_maximized_fullscreen_window(SDL_Window* window);

        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

        bool _owns_gamepad_subsystem = false;
        float _wheel_delta = 0.0F;
        MouseLockMode _requested_mouse_lock_mode = MouseLockMode::UNLOCKED;
        MouseLockMode _mouse_lock_mode = MouseLockMode::UNLOCKED;
        SDL_Window* _mouse_lock_window = nullptr;
    };
}
