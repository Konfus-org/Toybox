#pragma once
#include "tbx/common/result.h"
#include "tbx/graphics/api.h"
#include "tbx/graphics/window.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Supplies an API-specific loader for backend initialization.
    /// @details
    /// Ownership: Non-owning pointer; callers must ensure it remains valid.
    /// Thread Safety: Safe to copy; invocation must follow the provider's thread rules.
    using GraphicsProcAddress = void* (*)(const char*);

    /// @brief
    /// Purpose: Exposes active OpenGL context management for the engine render pipeline.
    /// @details
    /// Ownership: Implementations own backend window/context state.
    /// Thread Safety: Not inherently thread-safe; callers should follow the implementation rules.
    class TBX_API IOpenGlContextManager
    {
      public:
        IOpenGlContextManager();
        virtual ~IOpenGlContextManager() noexcept;

      public:
        virtual void initialize(
            int major_version,
            int minor_version,
            int depth_bits = 24,
            int stencil_bits = 8,
            bool double_buffer_enabled = true,
            bool debug_context_enabled = false,
            bool vsync_enabled = false) = 0;
        virtual Result make_current(const Window& window) = 0;
        virtual Result present(const Window& window) = 0;
        virtual Result set_vsync(const VsyncMode& mode) = 0;
        virtual GraphicsProcAddress get_proc_address() const = 0;
    };
}
