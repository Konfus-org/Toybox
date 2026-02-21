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

        Uuid window = {};
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

        Uuid window = {};
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

        Uuid window = {};
        GraphicsProcAddress get_proc_address = nullptr;
        Size size = {};
    };

    /// <summary>Notifies listeners that a window's native handle changed.</summary>
    /// <remarks>Purpose: Allows backend adapters to observe native window creation, recreation, and
    /// teardown without coupling windowing to a specific graphics backend.
    /// Ownership: Native handles are non-owning; providers must keep them valid while published.
    /// Thread Safety: Delivered on the dispatcher thread; consumers must obey backend thread
    /// rules.</remarks>
    struct TBX_API WindowNativeHandleChangedEvent : public Event
    {
        /// <summary>Creates a native-handle changed event for the specified window.</summary>
        /// <remarks>Purpose: Communicates old/new native handles so adapters can migrate resources.
        /// Ownership: Handles are non-owning; no lifetime extension is implied.
        /// Thread Safety: Safe to construct on any thread; handling is backend-thread
        /// specific.</remarks>
        WindowNativeHandleChangedEvent(
            const Uuid& window_id,
            void* previous_handle,
            void* handle,
            const Size& window_size)
            : window(window_id)
            , previous_native_handle(previous_handle)
            , native_handle(handle)
            , size(window_size)
        {
        }

        Uuid window = {};
        void* previous_native_handle = nullptr;
        void* native_handle = nullptr;
        Size size = {};
    };

    /// <summary>Requests that the windowing backend publish current native window
    /// handles.</summary> <remarks>Purpose: Allows adapters loaded after window creation to receive
    /// handle events. Ownership: No ownership transfer. Thread Safety: Should be handled on the
    /// main thread.</remarks>
    struct TBX_API WindowNativeHandleSnapshotRequest : public Request<void>
    {
        /// <summary>Creates a snapshot request.</summary>
        /// <remarks>Purpose: Prompts windowing to re-emit native-handle events for open windows.
        /// Ownership: No ownership transfer.
        /// Thread Safety: Safe to construct on any thread; handling should be
        /// main-thread.</remarks>
        WindowNativeHandleSnapshotRequest();

        /// <summary>Destroys the snapshot request instance.</summary>
        /// <remarks>Purpose: Cleans up request data.
        /// Ownership: No ownership transfer or release.
        /// Thread Safety: Safe to destroy on any thread.</remarks>
        ~WindowNativeHandleSnapshotRequest() noexcept override;
    };
}
