#pragma once
#include "tbx/input/input_messages.h"
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL.h>

namespace tbx::plugins
{
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
        void handle_controller_request(ControllerStateRequest& request) const;

        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

        bool _owns_gamepad_subsystem = false;
        float _wheel_delta = 0.0F;
    };
}
