#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <memory>

namespace sdl_input
{
    class SdlInputManager;

    /// @brief
    /// Purpose: Registers the SDL-backed input service and keeps backend state synchronized each
    /// frame.
    /// @details
    /// Ownership: Owns initialization state for the SDL gamepad subsystem.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.
    [[tbx::plugin]];
    [[tbx::name("SdlInputPlugin")]];
    [[tbx::version("1.0.0")]];
    [[tbx::category("input")]];
    class TBX_PLUGIN_API SdlInputPlugin final : public tbx::Plugin
    {
      public:
        void on_attach(tbx::ServiceProvider& service_provider) override;
        void on_detach(tbx::ServiceProvider& service_provider) override;
        void on_update(const tbx::DeltaTime& dt) override;

      private:
        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

      private:
        std::weak_ptr<SdlInputManager> _input_manager = {};
        bool _owns_gamepad_subsystem = false;
    };
}
