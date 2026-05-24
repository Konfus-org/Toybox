#pragma once
#include "tbx/plugins/sdl_base_systems/sdl_base_systems_plugin.h"
#include "tbx/systems/debugging/macros.h"
#include <SDL3/SDL.h>

namespace sdl_base_systems::internal
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
        // We don't really care about infos and debug messages from SDL
        /*else if (priority == SDL_LOG_PRIORITY_INFO)
        {
            const char* text =


         * * (message && *message) ? message : "SDL reported an info message without details.";


         * * TBX_TRACE_INFO("SDL info (category {}): {}", category, text);
        }
        else

         * {

         * const char* text =
                (message && *message) ? message : "SDL
         * reported a
         * debug message without details.";
            TBX_TRACE_INFO("SDL
         * debug (category {}):
         * {}", category, text);
        }*/
    }

}
