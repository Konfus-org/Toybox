#pragma once
#include "tbx/graphics/window.h"
#include "tbx/messages/message.h"

namespace tbx
{
    /// @brief
    /// Purpose: Supplies an API-specific loader for backend initialization.
    /// @details
    /// Ownership: Non-owning pointer; callers must ensure it remains valid.
    /// Thread Safety: Safe to copy; invocation must follow the provider's thread rules.
    using GraphicsProcAddress = void* (*)(const char*);

    /// @brief
    /// Purpose: Allows rendering backends to ensure the correct window context is active.
    /// @details
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.
    struct TBX_API WindowMakeCurrentRequest : public Request<void>
    {
        WindowMakeCurrentRequest(const Uuid& window_id);
        ~WindowMakeCurrentRequest() noexcept override;

        Uuid window = {};
    };

    /// @brief
    /// Purpose: Triggers swap/present for the specified window.
    /// @details
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.
    struct TBX_API WindowPresentRequest : public Request<void>
    {
        WindowPresentRequest(const Uuid& window_id);
        ~WindowPresentRequest() noexcept override;

        Uuid window = {};
    };

    /// @brief
    /// Purpose: Provides a loader function so backends can resolve API entry points.
    /// @details
    /// Ownership: The loader pointer is non-owning; it must remain valid for use.
    /// Thread Safety: Delivered on the dispatcher thread; use on the render thread.
    struct TBX_API WindowContextReadyEvent : public Event
    {
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

}
