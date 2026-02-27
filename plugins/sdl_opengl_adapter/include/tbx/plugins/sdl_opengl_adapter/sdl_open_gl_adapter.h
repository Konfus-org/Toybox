#pragma once
#include "tbx/graphics/messages.h"
#include "tbx/plugin_api/plugin_export.h"
#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef TBX_SDL_OPEN_GL_ADAPTER_PLUGIN_EXPORTS
    #define TBX_SDL_OPEN_GL_ADAPTER_PLUGIN_EXPORTS 0
#endif

namespace sdl_opengl_adapter
{
    using namespace tbx;
    /// <summary>Settings that control SDL OpenGL context creation and behavior.</summary>
    /// <remarks>Purpose: Captures OpenGL-specific configuration for SDL-backed windows.
    /// Ownership: Value type; callers own their instances.
    /// Thread Safety: Safe to copy; values are immutable unless explicitly reassigned.</remarks>
    struct TBX_PLUGIN_INCLUDE_API(TBX_SDL_OPEN_GL_ADAPTER_PLUGIN_EXPORTS) SdlOpenGlAdapterSettings
    {
        int major_version = 4;
        int minor_version = 5;
        int depth_bits = 24;
        int stencil_bits = 8;
        bool is_double_buffer_enabled = true;
        bool is_debug_context_enabled = false;
        bool is_vsync_enabled = false;
    };

    /// <summary>Bridges SDL windows and OpenGL context management for the engine.</summary>
    /// <remarks>Purpose: Centralizes SDL OpenGL attribute setup, context creation, and swap.
    /// Ownership: Owns one SDL_GLContext per tracked SDL window and releases them on destruction.
    /// Thread Safety: Not thread-safe; expected to be used from the render/main thread that owns
    /// the SDL contexts.</remarks>
    class TBX_PLUGIN_INCLUDE_API(TBX_SDL_OPEN_GL_ADAPTER_PLUGIN_EXPORTS) SdlOpenGlAdapter final
    {
      public:
        explicit SdlOpenGlAdapter(const SdlOpenGlAdapterSettings& settings);
        ~SdlOpenGlAdapter() noexcept;

        /// <summary>Updates runtime settings (vsync/debug) used by new and existing
        /// contexts.</summary> <remarks>Purpose: Allows plugins to keep context behavior aligned
        /// with app settings. Ownership: Copies the provided settings. Thread Safety: Not
        /// thread-safe; must be called on the owning thread.</remarks>
        void set_settings(const SdlOpenGlAdapterSettings& settings);

        /// <summary>Applies default SDL OpenGL attributes for future context creation.</summary>
        /// <remarks>Purpose: Configures SDL_GL attributes
        /// (version/profile/depth/stencil/doublebuffer). Ownership: No ownership transfer. Thread
        /// Safety: Not thread-safe; call before creating contexts.</remarks>
        void apply_default_attributes() const;

        /// <summary>Creates an OpenGL context for one SDL window when needed.</summary>
        /// <remarks>Purpose: Initializes and binds a per-window SDL_GLContext and applies current
        /// vsync setting.
        /// Ownership: Adapter owns the created context until `destroy_context` or destruction.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.</remarks>
        bool try_create_context(SDL_Window* sdl_window, const std::string& window_title);

        /// <summary>Destroys the OpenGL context associated with one SDL window.</summary>
        /// <remarks>Purpose: Releases one SDL_GLContext owned by this adapter.
        /// Ownership: Adapter releases its owned context; the SDL_Window remains non-owning.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.</remarks>
        void destroy_context(SDL_Window* sdl_window);

        /// <summary>Makes the OpenGL context for the given window current.</summary>
        /// <remarks>Purpose: Activates the window's context for subsequent GL commands.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.</remarks>
        bool try_make_current(SDL_Window* sdl_window, const std::string& window_title);

        /// <summary>Presents the window back buffer.</summary>
        /// <remarks>Purpose: Swaps the SDL window buffers for the stored context.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Not thread-safe; must be called on the owning thread.</remarks>
        bool try_present(SDL_Window* sdl_window);

        /// <summary>Applies the current vsync setting to all tracked contexts.</summary>
        /// <remarks>Purpose: Updates swap interval on all owned contexts while restoring prior
        /// current context. Ownership: The provided window pointers are non-owning. Thread Safety:
        /// Not thread-safe; must be called on the owning thread.</remarks>
        void apply_vsync_setting(const std::vector<SDL_Window*>& windows);

        /// <summary>Returns the SDL OpenGL proc-address loader function.</summary>
        /// <remarks>Purpose: Supplies a loader compatible with gladLoadGLLoader.
        /// Ownership: Non-owning pointer; valid as long as SDL is available.
        /// Thread Safety: Safe to copy; invocation must obey SDL context thread rules.</remarks>
        [[nodiscard]] GraphicsProcAddress get_proc_address() const;

        /// <summary>Returns whether a context exists for the given window.</summary>
        /// <remarks>Purpose: Allows callers to validate context availability.
        /// Ownership: Non-owning window pointer.
        /// Thread Safety: Not thread-safe; call on owning thread.</remarks>
        [[nodiscard]] bool has_context(SDL_Window* sdl_window) const;

      private:
        SdlOpenGlAdapterSettings _settings = {};
        std::unordered_map<SDL_Window*, SDL_GLContext> _contexts = {};
    };
}
