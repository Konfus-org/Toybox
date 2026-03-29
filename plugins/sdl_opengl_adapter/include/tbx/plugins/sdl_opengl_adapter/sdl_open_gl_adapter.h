#pragma once
#include "tbx/graphics/messages.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

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
        bool is_vsync_enabled = false;
    };

    /// @brief
    /// Purpose: Centralizes SDL OpenGL attribute setup, context creation, and swap.
    /// @details
    /// Ownership: Owns one SDL_GLContext per tracked SDL window and releases them on destruction.
    /// Thread Safety: Not thread-safe; expected to be used from the render/main thread that owns
    /// the SDL contexts.
    class TBX_PLUGIN_API SdlOpenGlAdapter final
    {
      public:
        explicit SdlOpenGlAdapter(const SdlOpenGlAdapterSettings& settings);
        ~SdlOpenGlAdapter() noexcept;

        /// @brief
        /// Purpose: Updates runtime settings (vsync/debug) used by new and existing contexts.
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
        /// Purpose: Initializes and binds a per-window SDL_GLContext and applies current vsync
        /// setting.
        /// @details
        /// Ownership: Adapter owns the created context until `destroy_context` or destruction.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        bool try_create_context(SDL_Window* sdl_window, const std::string& window_title);

        /// @brief
        /// Purpose: Releases one SDL_GLContext owned by this adapter.
        /// @details
        /// Ownership: Adapter releases its owned context; the SDL_Window remains non-owning.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        void destroy_context(SDL_Window* sdl_window);

        /// @brief
        /// Purpose: Activates the window's context for subsequent GL commands.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        bool try_make_current(SDL_Window* sdl_window, const std::string& window_title);

        /// @brief
        /// Purpose: Swaps the SDL window buffers for the stored context.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        bool try_present(SDL_Window* sdl_window);

        /// @brief
        /// Purpose: Applies the current vsync setting to all tracked contexts.
        /// @details
        /// Ownership: The provided window pointers are non-owning.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.
        void apply_vsync_setting(const std::vector<SDL_Window*>& windows);

        /// @brief
        /// Purpose: Supplies a loader compatible with gladLoadGLLoader.
        /// @details
        /// Ownership: Non-owning pointer; valid as long as SDL is available.
        /// Thread Safety: Safe to copy; invocation must obey SDL context thread rules.
        [[nodiscard]] tbx::GraphicsProcAddress get_proc_address() const;

        /// @brief
        /// Purpose: Allows callers to validate context availability.
        /// @details
        /// Ownership: Non-owning window pointer.
        /// Thread Safety: Not thread-safe; call on owning thread.
        [[nodiscard]] bool has_context(SDL_Window* sdl_window) const;

      private:
        SdlOpenGlAdapterSettings _settings = {};
        std::unordered_map<SDL_Window*, SDL_GLContext> _contexts = {};
    };
}
