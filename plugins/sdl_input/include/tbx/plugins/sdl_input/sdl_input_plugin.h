#pragma once
#include "tbx/input/manager.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>

namespace sdl_input
{
    class SdlInputManager;

    /// @brief
    /// Purpose: Registers the SDL-backed input service and keeps backend state synchronized each
    /// frame.
    /// @details
    /// Ownership: Owns initialization state for the SDL gamepad subsystem.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.
    class TBX_PLUGIN_API SdlInputPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

      private:
        tbx::ServiceProvider* _service_provider = nullptr;
        SdlInputManager* _input_manager = nullptr;
        bool _owns_gamepad_subsystem = false;
    };
}
