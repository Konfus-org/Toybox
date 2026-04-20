#pragma once
#include "tbx/graphics/window.h"
#include "tbx/messages/message.h"
#include <string>

namespace tbx
{
    /// @brief
    /// Purpose: Supplies an API-specific loader for backend initialization.
    /// @details
    /// Ownership: Non-owning pointer; callers must ensure it remains valid.
    /// Thread Safety: Safe to copy; invocation must follow the provider's thread rules.
    using GraphicsProcAddress = void* (*)(const char*);

    /// @brief
    /// Purpose: Signals that a managed window has been opened.
    /// @details
    /// Ownership: Copies the window id by value; no ownership transfer.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowOpenedEvent : public Event
    {
        WindowOpenedEvent(const Window& window_id);
        ~WindowOpenedEvent() noexcept override;

        Window window = {};
    };

    /// @brief
    /// Purpose: Signals that a managed window has been closed.
    /// @details
    /// Ownership: Copies the window id by value; no ownership transfer.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowClosedEvent : public Event
    {
        WindowClosedEvent(const Window& window_id);
        ~WindowClosedEvent() noexcept override;

        Window window = {};
    };

    /// @brief
    /// Purpose: Signals that a managed window title changed.
    /// @details
    /// Ownership: Copies the window id and title strings by value.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowTitleChangedEvent : public Event
    {
        WindowTitleChangedEvent(
            const Window& window_id,
            std::string previous_title,
            std::string current_title);
        ~WindowTitleChangedEvent() noexcept override;

        Window window = {};
        std::string previous = {};
        std::string current = {};
    };

    /// @brief
    /// Purpose: Signals that a managed window size changed.
    /// @details
    /// Ownership: Copies the window id and size values by value.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowSizeChangedEvent : public Event
    {
        WindowSizeChangedEvent(
            const Window& window_id,
            const Size& previous_size,
            const Size& current_size);
        ~WindowSizeChangedEvent() noexcept override;

        Window window = {};
        Size previous = {};
        Size current = {};
    };

    /// @brief
    /// Purpose: Signals that a managed window mode changed.
    /// @details
    /// Ownership: Copies the window id and enum values by value.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowModeChangedEvent : public Event
    {
        WindowModeChangedEvent(
            const Window& window_id,
            WindowMode previous_mode,
            WindowMode current_mode);
        ~WindowModeChangedEvent() noexcept override;

        Window window = {};
        WindowMode previous = WindowMode::WINDOWED;
        WindowMode current = WindowMode::WINDOWED;
    };

    /// @brief
    /// Purpose: Signals that a managed window native handle changed.
    /// @details
    /// Ownership: Copies the window id and non-owning handle values by value.
    /// Thread Safety: Delivered on the dispatcher thread.
    struct TBX_API WindowNativeHandleChangedEvent : public Event
    {
        WindowNativeHandleChangedEvent(
            const Window& window_id,
            NativeWindowHandle previous_handle,
            NativeWindowHandle current_handle);
        ~WindowNativeHandleChangedEvent() noexcept override;

        Window window = {};
        NativeWindowHandle previous = nullptr;
        NativeWindowHandle current = nullptr;
    };

    /// @brief
    /// Purpose: Allows rendering backends to ensure the correct window context is active.
    /// @details
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.
    struct TBX_API WindowMakeCurrentRequest : public Request<void>
    {
        WindowMakeCurrentRequest(const Window& window_id);
        ~WindowMakeCurrentRequest() noexcept override;

        Window window = {};
    };

    /// @brief
    /// Purpose: Triggers swap/present for the specified window.
    /// @details
    /// Ownership: The window ID is copied; no ownership transfer.
    /// Thread Safety: Should be handled on the render thread.
    struct TBX_API WindowPresentRequest : public Request<void>
    {
        WindowPresentRequest(const Window& window_id);
        ~WindowPresentRequest() noexcept override;

        Window window = {};
    };

    /// @brief
    /// Purpose: Provides a loader function so backends can resolve API entry points.
    /// @details
    /// Ownership: The loader pointer is non-owning; it must remain valid for use.
    /// Thread Safety: Delivered on the dispatcher thread; use on the render thread.
    struct TBX_API WindowContextReadyEvent : public Event
    {
        WindowContextReadyEvent(
            const Window& window_id,
            GraphicsProcAddress loader,
            const Size& window_size);
        ~WindowContextReadyEvent() noexcept override;

        Window window = {};
        GraphicsProcAddress get_proc_address = nullptr;
        Size size = {};
    };
}
