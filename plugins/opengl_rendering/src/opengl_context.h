#pragma once
#include "tbx/graphics/render_pipeline.h"
#include "tbx/graphics/window.h"
#include <functional>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores the target window identifier and routes context operations through the
    /// registered graphics context manager service.
    /// @details
    /// Ownership: Stores a non-owning context-manager reference and a copied window identifier.
    /// Thread Safety: Not thread-safe; use from the render thread that owns OpenGL calls.
    class OpenGlContext final
    {
      public:
        OpenGlContext(tbx::IGraphicsContextManager& context_manager, const tbx::Window& window_id);

      public:
        /// @brief
        /// Purpose: Exposes the bound window id for diagnostics and routing.
        /// @details
        /// Ownership: Returns a const reference to the context-owned id.
        /// Thread Safety: Read-only access is thread-compatible; operations remain render-thread
        /// only.
        const tbx::Window& get_window_id() const;

        /// @brief
        /// Purpose: Makes the bound window's context current through the context-manager service.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.
        tbx::Result make_current() const;

        /// @brief
        /// Purpose: Presents the bound window through the context-manager service.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.
        tbx::Result present() const;

      private:
        std::reference_wrapper<tbx::IGraphicsContextManager> _context_manager;
        tbx::Window _window_id = {};
    };
}
