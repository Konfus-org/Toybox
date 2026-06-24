#pragma once
#include "sdl_input_backend.h"
#include "tbx/interfaces/input_backend.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <memory>

namespace sdl_input
{
    /// @brief
    /// Purpose: Registers the SDL-backed input service and keeps backend state synchronized each
    /// frame.
    /// @details
    /// Ownership: Owns initialization state for the SDL gamepad subsystem.
    /// Thread Safety: Not thread-safe; expected to run on the main thread.
    [[tbx::register_plugin(
        name = "SdlInput",
        version = "1.0.0",
        category = tbx::PluginCategory::INPUT)]];
    class TBX_PLUGIN_API SdlInput final : public tbx::Plugin
    {
      protected:
        void on_attach() override;
        void on_detach() override;
        void on_update(const tbx::DeltaTime& dt) override;

      public:
        [[tbx::register(tbx::IInputBackend)]]
        std::weak_ptr<SdlInputBackend> input_backend = {};

      private:
        static bool accumulate_wheel_delta(void* userdata, SDL_Event* event);

        bool _owns_gamepad_subsystem = false;
    };
}
