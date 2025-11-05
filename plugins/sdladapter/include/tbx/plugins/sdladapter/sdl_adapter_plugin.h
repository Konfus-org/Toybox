#pragma once
#include "tbx/plugin_api/plugin.h"
#include <SDL3/SDL_log.h>

namespace tbx::plugins::sdladapter
{
    class SdlAdapterPlugin final : public Plugin
    {
       public:
        void on_attach(const ApplicationContext& context) override;
        void on_detach() override;
        void on_update(const DeltaTime& dt) override;
        void on_message(const Message& msg) override;

       private:
        void install_error_hook();
        void remove_error_hook();

        bool _initialized = false;
        bool _owns_sdl = false;
        void* _previous_log_userdata = nullptr;
        bool _log_hook_installed = false;
        SDL_LogOutputFunction _previous_log_callback = nullptr;
    };
}
