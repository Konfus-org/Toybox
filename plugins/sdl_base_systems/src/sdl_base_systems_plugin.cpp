#include "sdl_base_systems_plugin.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL_stdinc.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/time/delta_time.h"

namespace sdl_base_systems
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
    }

    void SdlBaseSystems::on_attach()
    {
        SDL_SetLogOutputFunction(
            [](void* userdata, int category, SDL_LogPriority priority, const char* message)
            {
                sdl_log_callback(userdata, category, priority, message);
            },
            this);

        const Uint32 mask = SDL_INIT_EVENTS;
        if (!SDL_InitSubSystem(mask))
        {
            TBX_ASSERT(
                false,
                "SDL base systems failed to initialize events subsystem. See SDL logs for "
                "details.");
            return;
        }

        TBX_TRACE_INFO("SDL base systems initialized the SDL events subsystem.");
    }

    void SdlBaseSystems::on_detach()
    {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }

    void SdlBaseSystems::on_update(const tbx::DeltaTime&)
    {
        SDL_PumpEvents();
    }
}
