#pragma once
#include "tbx/interfaces/window_backend.h"
#include <SDL3/SDL.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sdl_windowing
{
    class SdlWindowBackend final : public tbx::IWindowBackend
    {
      public:
        bool create_window(
            const tbx::Window& window,
            const tbx::WindowCreateInfo& create_info,
            tbx::NativeWindowHandle& out_native_handle) override;
        bool destroy_window(const tbx::Window& window) override;
        bool set_window_mode(const tbx::Window& window, tbx::WindowMode mode) override;
        bool set_window_title(const tbx::Window& window, const std::string& title) override;
        bool set_window_size(const tbx::Window& window, const tbx::Size& size) override;
        void pump_events(std::vector<tbx::WindowBackendEvent>& out_events) override;
        void shutdown() override;

        void set_icon_surface(SDL_Surface* icon_surface);
        void set_use_opengl(bool use_opengl);

      private:
        SDL_Window* create_sdl_window(
            const tbx::Window& window,
            const tbx::WindowCreateInfo& create_info) const;
        std::optional<tbx::Window> try_get_window_id(const SDL_Window* sdl_window) const;

      private:
        std::unordered_map<tbx::Window, SDL_Window*> _windows = {};
        SDL_Surface* _icon_surface = nullptr;
        bool _use_opengl = false;
    };
}
