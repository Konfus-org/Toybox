#pragma once
#include "tbx/input/manager.h"
#include <SDL3/SDL.h>
#include <string>

namespace sdl_input
{
    /// @brief
    /// Purpose: Implements the engine input service by translating SDL device state into Toybox
    /// input snapshots.
    /// @details
    /// Ownership: Owns SDL-specific mouse lock state for the active backend.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.
    class SdlInputManager final : public tbx::InputManager
    {
      public:
        SdlInputManager() = default;
        ~SdlInputManager() noexcept override;

      public:
        tbx::KeyboardState get_keyboard_state() const override;
        tbx::ControllerState get_controller_state(int controller_index) const override;
        tbx::MouseState get_mouse_state() const override;
        void set_mouse_lock_mode(tbx::MouseLockMode mode) override;
        tbx::MouseLockMode get_mouse_lock_mode() const override;

        void add_wheel_delta(float wheel_delta);
        void update_backend_state();

      private:
        bool apply_mouse_lock_mode(std::string* out_error_report = nullptr);
        bool release_mouse_lock_window(std::string* out_error_report = nullptr);
        static bool is_maximized_fullscreen_window(SDL_Window* window);

      private:
        float _wheel_delta = 0.0F;
        tbx::MouseLockMode _requested_mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        tbx::MouseLockMode _mouse_lock_mode = tbx::MouseLockMode::UNLOCKED;
        SDL_Window* _mouse_lock_window = nullptr;
    };
}
