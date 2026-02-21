#pragma once
#include "tbx/common/result.h"
#include "tbx/common/uuid.h"
#include "tbx/messages/dispatcher.h"

namespace tbx::plugins
{
    /// <summary>Represents an OpenGL window context bound to dispatcher-driven window
    /// requests.</summary>
    /// <remarks>Purpose: Stores the target window identifier and exposes helper calls to make the
    /// context current and present through the message dispatcher.
    /// Ownership: Stores a non-owning dispatcher pointer and a copied window identifier.
    /// Thread Safety: Not thread-safe; use from the render thread that owns OpenGL calls.</remarks>
    class OpenGlContext final
    {
      public:
        OpenGlContext() = default;

        /// <summary>Creates a context helper for one window.</summary>
        /// <remarks>Purpose: Binds window context operations to a dispatcher and window id.
        /// Ownership: Keeps a non-owning dispatcher pointer; caller must keep dispatcher alive.
        /// Thread Safety: Safe to construct on one thread, use on render thread.</remarks>
        OpenGlContext(IMessageDispatcher& dispatcher, const Uuid& window_id);

        /// <summary>Returns the window identifier targeted by this context.</summary>
        /// <remarks>Purpose: Exposes the bound window id for diagnostics and routing.
        /// Ownership: Returns a const reference to context-owned id.
        /// Thread Safety: Read-only access is thread-compatible; operations remain render-thread
        /// only.</remarks>
        const Uuid& get_window_id() const;

        /// <summary>Requests that the bound window context become current.</summary>
        /// <remarks>Purpose: Routes a make-current request through the dispatcher.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.</remarks>
        Result make_current() const;

        /// <summary>Requests present/swap for the bound window context.</summary>
        /// <remarks>Purpose: Routes a present request through the dispatcher.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Call on render thread.</remarks>
        Result present() const;

      private:
        IMessageDispatcher* _dispatcher = nullptr;
        Uuid _window_id = Uuid::NONE;
    };
}
