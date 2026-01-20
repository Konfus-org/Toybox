#pragma once
#include "tbx/graphics/window.h"
#include "tbx/messages/message.h"

namespace tbx
{
    /// <summary>Function pointer used to resolve graphics API entry points.</summary>
    /// <remarks>Purpose: Supplies an API-specific loader for backend initialization.
    /// Ownership: Non-owning pointer; callers must ensure it remains valid.
    /// Thread Safety: Safe to copy; invocation must follow the provider's thread rules.</remarks>
    using GraphicsProcAddress = void* (*)(const char*);

    /// <summary>Requests that a window's graphics context become current.</summary>
    /// <remarks>Purpose: Allows rendering backends to ensure the correct window context is active.
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.</remarks>
    struct TBX_API WindowMakeCurrentRequest : public Request<void>
    {
        /// <summary>Creates a request for the specified window.</summary>
        /// <remarks>Purpose: Targets a specific window by its identifier.
        /// Ownership: Copies the identifier value.
        /// Thread Safety: Safe to construct on any thread; handling is render-thread
        /// only.</remarks>
        WindowMakeCurrentRequest(const Uuid& window_id);

        /// <summary>Destroys the request instance.</summary>
        /// <remarks>Purpose: Cleans up the request data.
        /// Ownership: No ownership transfer or release.
        /// Thread Safety: Safe to destroy on any thread.</remarks>
        ~WindowMakeCurrentRequest() noexcept override;

        Uuid window = invalid::uuid;
    };

    /// <summary>Requests that a window present its back buffer.</summary>
    /// <remarks>Purpose: Triggers swap/present for the specified window.
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.</remarks>
    struct TBX_API WindowPresentRequest : public Request<void>
    {
        /// <summary>Creates a present request for the specified window.</summary>
        /// <remarks>Purpose: Targets a specific window by its identifier.
        /// Ownership: Copies the identifier value.
        /// Thread Safety: Safe to construct on any thread; handling is render-thread
        /// only.</remarks>
        WindowPresentRequest(const Uuid& window_id);

        /// <summary>Destroys the present request instance.</summary>
        /// <remarks>Purpose: Cleans up the request data.
        /// Ownership: No ownership transfer or release.
        /// Thread Safety: Safe to destroy on any thread.</remarks>
        ~WindowPresentRequest() noexcept override;

        Uuid window = invalid::uuid;
    };

    /// <summary>Notifies listeners that a window context is ready.</summary>
    /// <remarks>Purpose: Provides a loader function so backends can resolve API entry points.
    /// Ownership: The loader pointer is non-owning; it must remain valid for use.
    /// Thread Safety: Delivered on the dispatcher thread; use on the render thread.</remarks>
    struct TBX_API WindowContextReadyEvent : public Event
    {
        /// <summary>Creates a context-ready event for the specified window.</summary>
        /// <remarks>Purpose: Shares the loader function for API initialization.
        /// Ownership: Copies the identifier and loader pointer.
        /// Thread Safety: Safe to construct on any thread; handling is render-thread
        /// only.</remarks>
        WindowContextReadyEvent(
            const Uuid& window_id,
            GraphicsProcAddress loader,
            const Size& window_size)
            : window(window_id)
            , get_proc_address(loader)
            , size(window_size)
        {
        }

        Uuid window = invalid::uuid;
        GraphicsProcAddress get_proc_address = nullptr;
        Size size = {};
    };
}
