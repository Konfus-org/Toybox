#pragma once
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"
#include <functional>

namespace opengl_rendering
{
    /// @brief
    /// Purpose: Stores the target window identifier and exposes helper calls to make the context
    /// current and present through the message dispatcher.
    /// @details
    /// Ownership: Stores a non-owning dispatcher pointer and a copied window identifier.
    /// Thread Safety: Not thread-safe; use from the render thread that owns OpenGL calls.
    class OpenGlContext final
    {
      public:
        OpenGlContext(tbx::IMessageDispatcher& dispatcher, const tbx::Uuid& window_id);

        /// @brief
        /// Purpose: Exposes the bound window id for diagnostics and routing.
        /// @details
        /// Ownership: Returns a const reference to context-owned id.
        /// Thread Safety: Read-only access is thread-compatible; operations remain render-thread
        /// only.
        const tbx::Uuid& get_window_id() const;

        /// @brief
        /// Purpose: Routes a make-current request through the dispatcher.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.
        tbx::Result make_current() const;

        /// @brief
        /// Purpose: Routes a present request through the dispatcher.
        /// @details
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.
        tbx::Result present() const;

      private:
        std::reference_wrapper<tbx::IMessageDispatcher> _dispatcher;
        tbx::Uuid _window_id = tbx::Uuid::NONE;
    };
}
