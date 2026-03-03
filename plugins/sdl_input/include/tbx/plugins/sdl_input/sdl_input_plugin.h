#pragma once
#include "tbx/input/input_messages.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <string>


namespace sdl_input
{
    /// <summary>Provides SDL-backed keyboard, mouse, and controller state requests.</summary>
    /// <remarks>Purpose: Handles core input requests and translates SDL state into engine messages.
    /// Ownership: Owns initialization state for the SDL gamepad subsystem.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.</remarks>
    class TBX_PLUGIN_API SdlInputPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::IPluginHost& host) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;
        void on_recieve_message(tbx::Message& msg) override;

      private:
        void handle_keyboard_request(tbx::KeyboardStateRequest& request) const;
        void handle_mouse_request(tbx::MouseStateRequest& request);
        void handle_set_mouse_lock_request(tbx::SetMouseLockRequest& request);
        void handle_mouse_lock_mode_request(tbx::MouseLockModeRequest& request) const;
        void handle_controller_request(tbx::ControllerStateRequest& request) const;
        bool apply_mouse_lock_mode(std::string* out_error_report = nullptr);
        bool release_mouse_lock_window(std::string* out_error_report = nullptr);
        static bool is_maximized_fullscreen_window(SDL_Window* window);

        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

        bool _owns_gamepad_subsystem = false;
        float _wheel_delta = 0.0F;
        tbx::MouseLockMode _requested_mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        tbx::MouseLockMode _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        SDL_Window* _mouse_lock_window = nullptr;
    };
}
