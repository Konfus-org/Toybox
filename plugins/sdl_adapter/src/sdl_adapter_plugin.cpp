#include "sdl_adapter_plugin.h"
#include "tbx/debugging/macros.h"
#include <SDL3/SDL.h>

namespace tbx::plugins::sdladapter
{
    void SdlAdapterPlugin::on_attach(Application&)
    {
        if (_initialized)
        {
            return;
        }

        install_error_hook();

        const Uint32 mask = SDL_INIT_EVENTS;
        if ((SDL_WasInit(mask) & mask) == mask)
        {
            _initialized = true;
            _owns_sdl = false;
            TBX_TRACE_WARNING(
                "SDL events subsystem already initialized; adapter will not manage shutdown.");
            return;
        }

        if (!SDL_InitSubSystem(mask))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL events subsystem. See SDL logs for details.");
            TBX_ASSERT(
                false,
                "SDL adapter failed to initialize events subsystem. See SDL logs for details.");
            _initialized = false;
            _owns_sdl = false;
            return;
        }

        _initialized = true;
        _owns_sdl = true;
        TBX_TRACE_INFO("SDL adapter initialized the events subsystem.");
    }

    void SdlAdapterPlugin::on_detach()
    {
        if (_initialized && _owns_sdl)
        {
            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        }

        _initialized = false;
        _owns_sdl = false;

        remove_error_hook();
    }

    void SdlAdapterPlugin::on_update(const DeltaTime&)
    {
        SDL_PumpEvents();
    }

    void SdlAdapterPlugin::on_message(Message&) {}

    void SdlAdapterPlugin::install_error_hook()
    {
        if (_log_hook_installed)
        {
            return;
        }

        SDL_GetLogOutputFunction(&_previous_log_callback, &_previous_log_userdata);
        SDL_SetLogOutputFunction(
            [](void* userdata, int category, SDL_LogPriority priority, const char* message)
            {
                auto* plugin = static_cast<SdlAdapterPlugin*>(userdata);

                if (priority >= SDL_LOG_PRIORITY_ERROR)
                {
                    const char* text =
                        (message && *message) ? message : "SDL reported an error without details.";
                    if (priority == SDL_LOG_PRIORITY_CRITICAL)
                    {
                        TBX_TRACE_ERROR("SDL critical (category {}): {}", category, text);
                    }
                    else
                    {
                        TBX_TRACE_ERROR("SDL error (category {}): {}", category, text);
                    }
                }

                if (plugin && plugin->_previous_log_callback)
                {
                    plugin->_previous_log_callback(
                        plugin->_previous_log_userdata,
                        category,
                        priority,
                        message);
                }
            },
            this);
        _log_hook_installed = true;
    }

    void SdlAdapterPlugin::remove_error_hook()
    {
        if (!_log_hook_installed)
        {
            return;
        }

        SDL_SetLogOutputFunction(_previous_log_callback, _previous_log_userdata);
        _previous_log_callback = nullptr;
        _previous_log_userdata = nullptr;
        _log_hook_installed = false;
    }
}
