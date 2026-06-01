#pragma once
#include "tbx/interfaces/window_backend.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>

namespace sdl_windowing
{
    struct SdlSurfaceDeleter
    {
        void operator()(SDL_Surface* surface) const;
    };

    using SdlSurfacePtr = std::unique_ptr<SDL_Surface, SdlSurfaceDeleter>;

    class SdlWindowBackend final : public tbx::IWindowBackend
    {
      public:
        ~SdlWindowBackend() noexcept override;

      public:
        void initialize() override;
        void shutdown() override;
        void pump_events(std::vector<tbx::WindowBackendEvent>& out_events) override;

        bool create_window(
            const tbx::Window& window,
            const tbx::WindowCreateInfo& create_info,
            tbx::NativeWindowHandle& out_native_handle) override;
        bool destroy_window(const tbx::Window& window) override;
        bool set_window_mode(const tbx::Window& window, tbx::WindowMode mode) override;
        bool set_window_title(const tbx::Window& window, const std::string& title) override;
        bool set_window_size(const tbx::Window& window, const tbx::Size& size) override;

      private:
        SDL_Window* create_sdl_window(
            const tbx::Window& window,
            const tbx::WindowCreateInfo& create_info);
        void try_set_window_icon(
            const tbx::Window& window,
            SDL_Window* native_window,
            const std::filesystem::path& icon_path);
        std::optional<tbx::Window> try_get_window_id(const SDL_Window* sdl_window) const;

      private:
        std::unordered_map<tbx::Window, SDL_Window*> _windows = {};
        std::unordered_map<tbx::Window, SdlSurfacePtr> _icon_surfaces = {};
        bool _is_initialized = false;
    };
}
