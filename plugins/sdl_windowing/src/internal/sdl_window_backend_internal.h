#pragma once
#include "sdl_window_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/handle.h"
#include "tbx/types/typedefs.h"
#include <ranges>
#include <string_view>

namespace sdl_windowing::internal
{
    static bool is_wayland_video_driver()
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        return video_driver != nullptr && std::string_view(video_driver) == "wayland";
    }

    static void try_apply_window_icon(SDL_Window* native, SDL_Surface* icon_surface)
    {
        if (!native || !icon_surface)
            return;

        if (is_wayland_video_driver())
            return;

        if (!SDL_SetWindowIcon(native, icon_surface))
        {
            TBX_TRACE_WARNING("Failed to set SDL window icon. Error: {}", SDL_GetError());
            SDL_ClearError();
        }
    }

}
