#pragma once
#include "tbx/graphics/render_pipeline.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <unordered_map>

namespace sdl_opengl_adapter
{
    /// @brief
    /// Purpose: Captures OpenGL-specific configuration for SDL-backed windows.
    /// @details
    /// Ownership: Value type; callers own their instances.
    /// Thread Safety: Safe to copy; values are immutable unless explicitly reassigned.
    struct SdlOpenGlAdapterSettings
    {
        int major_version = 4;
        int minor_version = 5;
        int depth_bits = 24;
        int stencil_bits = 8;
        bool is_double_buffer_enabled = true;
        bool is_debug_context_enabled = false;
    };

    struct SdlOpenGlContextRecord
    {
        tbx::Window window = {};
        SDL_Window* native_window = nullptr;
        SDL_GLContext context = nullptr;
    };

    /// @brief
    /// Purpose: Owns SDL OpenGL contexts and exposes them through `IGraphicsContextManager`.
    /// @details
    /// Ownership: Borrows the window manager and owns one SDL_GLContext per tracked window.
    /// Thread Safety: Not thread-safe; expected to be used from the host/render threads that own
    /// the SDL contexts.
    class TBX_PLUGIN_API SdlOpenGlAdapter final : public tbx::IGraphicsContextManager
    {
      public:
        SdlOpenGlAdapter(
            tbx::IWindowManager& window_manager,
            const SdlOpenGlAdapterSettings& settings);
        ~SdlOpenGlAdapter() noexcept override;

      public:
        /// @brief
        /// Purpose: Updates the SDL context settings used for future context creation.
        /// @details
        /// Ownership: Copies the provided settings.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        void set_settings(const SdlOpenGlAdapterSettings& settings);

        /// @brief
        /// Purpose: Applies default SDL OpenGL attributes for future context creation.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; call before creating contexts.
        void apply_default_attributes() const;

        /// @brief
        /// Purpose: Syncs a managed window to the adapter's tracked native-handle state.
        /// @details
        /// Ownership: The provided SDL window pointer remains non-owning.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        void sync_window(const tbx::Window& window, SDL_Window* native_window);

        /// @brief
        /// Purpose: Releases any context tracked for a managed window.
        /// @details
        /// Ownership: Adapter releases its owned SDL context; the SDL window remains external.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        void release_window(const tbx::Window& window);

      public:
        tbx::Result make_current(const tbx::Window& window) override;
        tbx::Result present(const tbx::Window& window) override;
        tbx::Result set_vsync(const tbx::VsyncMode& mode) override;
        tbx::GraphicsProcAddress get_proc_address() const override;

      private:
        tbx::Result ensure_context(
            const tbx::Window& window,
            SDL_Window* native_window,
            SdlOpenGlContextRecord& record);
        SDL_Window* get_native_window(const tbx::Window& window) const;
        int get_swap_interval() const;
        void release_all_contexts();
        void release_record(SdlOpenGlContextRecord& record);
        tbx::Result set_current_context(SdlOpenGlContextRecord& record) const;
        tbx::Result try_apply_vsync(SdlOpenGlContextRecord& record) const;

      private:
        tbx::IWindowManager& _window_manager;
        SdlOpenGlAdapterSettings _settings = {};
        tbx::VsyncMode _vsync_mode = tbx::VsyncMode::OFF;
        std::unordered_map<tbx::Window, SdlOpenGlContextRecord> _contexts = {};
    };
}
