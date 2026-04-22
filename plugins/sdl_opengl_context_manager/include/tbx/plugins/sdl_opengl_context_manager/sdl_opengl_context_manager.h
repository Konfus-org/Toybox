#pragma once
#include "tbx/interfaces/opengl_context_manager.h"
#include "tbx/systems/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>

namespace sdl_opengl_context_manager
{
    /// @brief
    /// Purpose: Captures OpenGL-specific configuration for SDL-backed windows.
    /// @details
    /// Ownership: Value type; callers own their instances.
    /// Thread Safety: Safe to copy; values are immutable unless explicitly reassigned.
    struct SdlOpenGlContextSettings
    {
        int major_version = 4;
        int minor_version = 5;
        int depth_bits = 24;
        int stencil_bits = 8;
        bool is_double_buffer_enabled = true;
        bool is_debug_context_enabled = false;
        tbx::VsyncMode vsync_mode = tbx::VsyncMode::OFF;
    };

    /// @brief
    /// Purpose: Provides SDL-backed OpenGL context management through the graphics service API.
    /// @details
    /// Ownership: Owns one SDL_GLContext per tracked SDL window and releases them on destruction.
    /// Thread Safety: Not thread-safe; expected to be used from the render/main thread that owns
    /// the SDL contexts.
    class TBX_PLUGIN_API SdlOpenGlContextManager final : public tbx::IOpenGlContextManager
    {
      public:
        SdlOpenGlContextManager(tbx::IWindowManager& window_manager);
        ~SdlOpenGlContextManager() noexcept override;

        void initialize(
            int major_version,
            int minor_version,
            int depth_bits = 24,
            int stencil_bits = 8,
            bool double_buffer_enabled = true,
            bool debug_context_enabled = false,
            bool vsync_enabled = false) override;
        void shutdown() override;

        tbx::Result create_context(const tbx::Window& window) override;
        tbx::Result destroy_context(const tbx::Window& window) override;
        tbx::Result make_context_current(const tbx::Window& window) override;
        tbx::Result swap_buffers(const tbx::Window& window) override;

        tbx::Result set_vsync(const tbx::VsyncMode& mode) override;
        tbx::GraphicsProcAddress get_proc_address() const override;

      private:
        void apply_default_attributes() const;
        int get_swap_interval() const;
        tbx::Result make_failure(std::string message) const;
        SDL_Window* get_sdl_window(const tbx::Window& window) const;
        bool try_create_context(
            const tbx::Window& window,
            SDL_Window* sdl_window,
            const std::string& window_title);
        void destroy_native_context(const tbx::Window& window);
        bool try_make_current(
            SDL_Window* sdl_window,
            SDL_GLContext context,
            const std::string& window_title);
        bool try_present(const tbx::Window& window, SDL_Window* sdl_window);
        void apply_vsync_setting();

      private:
        tbx::IWindowManager& _window_manager;
        SdlOpenGlContextSettings _settings = {};
        std::unordered_map<tbx::Window, SDL_GLContext> _contexts = {};
    };
}
