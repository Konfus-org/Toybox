#pragma once
#include "sdl_window_backend.h"
#include "tbx/plugins/sdl_windowing/sdl_windowing_plugin.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/messaging/observable.h"
#include "tbx/systems/windowing/window_manager.h"
#include <filesystem>
#include <memory>
#include <string_view>

namespace sdl_windowing::internal
{
    static bool is_wayland_video_driver_for_plugin()
    {
        const char* video_driver = SDL_GetCurrentVideoDriver();
        return video_driver != nullptr && std::string_view(video_driver) == "wayland";
    }

    static SDL_Surface* try_load_icon_surface(const std::filesystem::path& icon_path)
    {
        if (icon_path.empty())
            return nullptr;

        if (is_wayland_video_driver_for_plugin())
            return nullptr;

        SDL_ClearError();
        if (SDL_Surface* icon_surface = SDL_LoadSurface(icon_path.string().c_str()))
        {
            TBX_TRACE_INFO("Loaded app icon '{}'.", icon_path.string());
            return icon_surface;
        }

        TBX_TRACE_WARNING(
            "Failed to load app icon '{}'. Error: {}",
            icon_path.string(),
            SDL_GetError());
        SDL_ClearError();
        return nullptr;
    }

}
