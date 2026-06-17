#include "sdl_window_backend.h"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/api.h"
#include "tbx/types/size.h"
#include "tbx/types/window.h"
#include <ranges>
#include <string_view>

namespace sdl_windowing
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

    static SdlSurfacePtr try_load_icon_surface(const std::filesystem::path& icon_path)
    {
        if (icon_path.empty())
            return nullptr;

        if (is_wayland_video_driver())
            return nullptr;

        SDL_ClearError();
        if (SDL_Surface* icon_surface = SDL_LoadSurface(icon_path.string().c_str()))
        {
            TBX_TRACE_INFO("Loaded app icon '{}'.", icon_path.string());
            return SdlSurfacePtr(icon_surface);
        }

        TBX_TRACE_WARNING(
            "Failed to load app icon '{}'. Error: {}",
            icon_path.string(),
            SDL_GetError());
        SDL_ClearError();
        return nullptr;
    }

    void SdlSurfaceDeleter::operator()(SDL_Surface* surface) const
    {
        if (surface)
            SDL_DestroySurface(surface);
    }

    SdlWindowBackend::~SdlWindowBackend() noexcept
    {
        shutdown();
    }

    void SdlWindowBackend::initialize()
    {
        if (_is_initialized)
            return;

        if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
        {
            TBX_TRACE_ERROR("Failed to initialize SDL video subsystem. Error: {}", SDL_GetError());
            SDL_ClearError();
            return;
        }

        _is_initialized = true;
        TBX_TRACE_INFO("Initialized SDL video subsystem.");
        TBX_TRACE_INFO("Video driver: {}", SDL_GetCurrentVideoDriver());
    }

    void SdlWindowBackend::shutdown()
    {
        for (const auto& [window_id, native_window] : _windows)
        {
            (void)window_id;
            if (native_window)
                SDL_DestroyWindow(native_window);
        }
        _windows.clear();
        _icon_surfaces.clear();

        if (_is_initialized)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            _is_initialized = false;
        }
    }

    bool SdlWindowBackend::create_window(
        const tbx::Window& window,
        const tbx::WindowCreateInfo& create_info,
        tbx::NativeWindowHandle& out_native_handle)
    {
        out_native_handle = nullptr;
        if (!_is_initialized)
            initialize();
        if (!_is_initialized)
            return false;

        const auto existing_window = _windows.find(window);
        if (existing_window != _windows.end() && existing_window->second)
        {
            out_native_handle = static_cast<tbx::NativeWindowHandle>(existing_window->second);
            return true;
        }

        SDL_Window* sdl_window = create_sdl_window(window, create_info);
        if (!sdl_window)
            return false;

        _windows[window] = sdl_window;
        try_set_window_icon(window, sdl_window, create_info.icon_path);
        out_native_handle = static_cast<tbx::NativeWindowHandle>(sdl_window);
        return true;
    }

    bool SdlWindowBackend::destroy_window(const tbx::Window& window)
    {
        const auto window_it = _windows.find(window);
        if (window_it == _windows.end())
            return true;

        SDL_DestroyWindow(window_it->second);
        _windows.erase(window_it);
        _icon_surfaces.erase(window);
        return true;
    }

    bool SdlWindowBackend::set_window_mode(const tbx::Window& window, tbx::WindowMode mode)
    {
        const auto window_it = _windows.find(window);
        if (window_it == _windows.end() || !window_it->second)
            return false;

        SDL_Window* native_window = window_it->second;
        if (mode == tbx::WindowMode::BORDERLESS)
        {
            SDL_SetWindowFullscreen(native_window, false);
            SDL_SetWindowBordered(native_window, false);
            SDL_RestoreWindow(native_window);
        }
        else if (mode == tbx::WindowMode::FULLSCREEN)
        {
            SDL_SetWindowBordered(native_window, true);
            SDL_SetWindowFullscreen(native_window, true);
        }
        else if (mode == tbx::WindowMode::MINIMIZED)
        {
            SDL_MinimizeWindow(native_window);
        }
        else
        {
            SDL_SetWindowFullscreen(native_window, false);
            SDL_SetWindowBordered(native_window, true);
            SDL_RestoreWindow(native_window);
        }

        return true;
    }

    bool SdlWindowBackend::set_window_title(const tbx::Window& window, const std::string& title)
    {
        const auto window_it = _windows.find(window);
        if (window_it == _windows.end() || !window_it->second)
            return false;

        SDL_SetWindowTitle(window_it->second, title.c_str());
        return true;
    }

    bool SdlWindowBackend::set_window_size(const tbx::Window& window, const tbx::Size& size)
    {
        const auto window_it = _windows.find(window);
        if (window_it == _windows.end() || !window_it->second)
            return false;

        SDL_SetWindowSize(
            window_it->second,
            static_cast<int>(size.width),
            static_cast<int>(size.height));
        return true;
    }

    void SdlWindowBackend::pump_events(std::vector<tbx::WindowBackendEvent>& out_events)
    {
        if (!_is_initialized)
            return;

        SDL_Event event = {};
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                {
                    auto* native_window = SDL_GetWindowFromID(event.window.windowID);
                    const auto window = try_get_window_id(native_window);
                    if (!window.has_value())
                        break;

                    out_events.push_back(
                        tbx::WindowBackendEvent {
                            .type = tbx::WindowBackendEventType::CLOSE_REQUESTED,
                            .window = *window,
                        });
                    break;
                }

                case SDL_EVENT_WINDOW_RESIZED:
                {
                    auto* native_window = SDL_GetWindowFromID(event.window.windowID);
                    const auto window = try_get_window_id(native_window);
                    if (!window.has_value())
                        break;

                    out_events.push_back(
                        tbx::WindowBackendEvent {
                            .type = tbx::WindowBackendEventType::RESIZED,
                            .window = *window,
                            .size = tbx::Size(
                                static_cast<uint>(event.window.data1),
                                static_cast<uint>(event.window.data2)),
                        });
                    break;
                }

                case SDL_EVENT_WINDOW_MINIMIZED:
                {
                    auto* native_window = SDL_GetWindowFromID(event.window.windowID);
                    const auto window = try_get_window_id(native_window);
                    if (!window.has_value())
                        break;

                    out_events.push_back(
                        tbx::WindowBackendEvent {
                            .type = tbx::WindowBackendEventType::MINIMIZED,
                            .window = *window,
                        });
                    break;
                }

                case SDL_EVENT_WINDOW_RESTORED:
                {
                    auto* native_window = SDL_GetWindowFromID(event.window.windowID);
                    const auto window = try_get_window_id(native_window);
                    if (!window.has_value())
                        break;

                    out_events.push_back(
                        tbx::WindowBackendEvent {
                            .type = tbx::WindowBackendEventType::RESTORED,
                            .window = *window,
                        });
                    break;
                }

                case SDL_EVENT_QUIT:
                    out_events.push_back(
                        tbx::WindowBackendEvent {
                            .type = tbx::WindowBackendEventType::QUIT_REQUESTED,
                        });
                    break;

                default:
                    break;
            }
        }
    }

    SDL_Window* SdlWindowBackend::create_sdl_window(
        const tbx::Window& window,
        const tbx::WindowCreateInfo& create_info)
    {
        uint flags = create_info.api == tbx::GraphicsApi::OPEN_GL ? SDL_WINDOW_OPENGL : 0;
        if (create_info.mode == tbx::WindowMode::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;
        else if (create_info.mode == tbx::WindowMode::FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN;
        else if (create_info.mode == tbx::WindowMode::WINDOWED)
            flags |= SDL_WINDOW_RESIZABLE;
        else if (create_info.mode == tbx::WindowMode::MINIMIZED)
            flags |= SDL_WINDOW_MINIMIZED;
        else if (create_info.mode == tbx::WindowMode::HIDDEN)
            flags |= SDL_WINDOW_HIDDEN;

        SDL_Window* native_window = SDL_CreateWindow(
            create_info.title.c_str(),
            static_cast<int>(create_info.size.width),
            static_cast<int>(create_info.size.height),
            flags);
        if (!native_window)
        {
            TBX_TRACE_ERROR(
                "SDL window backend: failed to create native window '{}'. Error: {}",
                window,
                SDL_GetError());
            SDL_ClearError();
            return nullptr;
        }

        return native_window;
    }

    void SdlWindowBackend::try_set_window_icon(
        const tbx::Window& window,
        SDL_Window* native_window,
        const std::filesystem::path& icon_path)
    {
        auto icon_surface = try_load_icon_surface(icon_path);
        if (!icon_surface)
            return;

        try_apply_window_icon(native_window, icon_surface.get());
        _icon_surfaces[window] = std::move(icon_surface);
    }

    std::optional<tbx::Window> SdlWindowBackend::try_get_window_id(
        const SDL_Window* sdl_window) const
    {
        const auto window_it = std::ranges::find_if(
            _windows,
            [sdl_window](const auto& entry)
            {
                return entry.second == sdl_window;
            });
        if (window_it == _windows.end())
            return std::nullopt;

        auto window = window_it->first;
        window.native_handle = static_cast<tbx::NativeWindowHandle>(window_it->second);
        return window;
    }
}
