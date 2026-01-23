#include "sdl_adapter_plugin.h"
#include "tbx/debugging/macros.h"
#include <SDL3/SDL.h>

namespace tbx::plugins
{
    static void sdl_log_callback(
        void* userdata,
        int category,
        SDL_LogPriority priority,
        const char* message)
    {
        if (priority >= SDL_LOG_PRIORITY_ERROR)
        {
            const char* text =
                (message && *message) ? message : "SDL reported an error without details.";
            if (priority == SDL_LOG_PRIORITY_CRITICAL)
                TBX_TRACE_ERROR("SDL critical (category {}): {}", category, text);
            else
                TBX_TRACE_ERROR("SDL error (category {}): {}", category, text);
        }
        else if (priority == SDL_LOG_PRIORITY_WARN)
        {
            const char* text =
                (message && *message) ? message : "SDL reported a warning without details.";
            TBX_TRACE_WARNING("SDL warning (category {}): {}", category, text);
        }
        else if (priority == SDL_LOG_PRIORITY_INFO)
        {
            const char* text =
                (message && *message) ? message : "SDL reported an info message without details.";
            TBX_TRACE_INFO("SDL info (category {}): {}", category, text);
        }
        else
        {
            const char* text =
                (message && *message) ? message : "SDL reported a debug message without details.";
            TBX_TRACE_INFO("SDL debug (category {}): {}", category, text);
        }
    }

    void SdlAdapterPlugin::on_attach(Application&)
    {
        SDL_SetLogOutputFunction(
            [](void* userdata, int category, SDL_LogPriority priority, const char* message)
            {
                sdl_log_callback(userdata, category, priority, message);
            },
            this);

        const Uint32 mask = SDL_INIT_EVENTS;
        if ((SDL_WasInit(mask) & mask) == mask)
        {
            _owns_sdl = false;
            TBX_TRACE_WARNING(
                "SDL events subsystem already initialized; adapter will not manage shutdown.");
            return;
        }

        if (!SDL_InitSubSystem(mask))
        {
            _owns_sdl = false;
            TBX_TRACE_ERROR("Failed to initialize SDL events subsystem. See SDL logs for details.");
            TBX_ASSERT(
                false,
                "SDL adapter failed to initialize events subsystem. See SDL logs for details.");
            return;
        }

        TBX_TRACE_INFO("SDL adapter initialized the events subsystem.");
    }

    void SdlAdapterPlugin::on_detach()
    {
        if (_owns_sdl)
            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        _owns_sdl = false;
    }

    void SdlAdapterPlugin::on_update(const DeltaTime&)
    {
        SDL_PumpEvents();
    }
}
