#pragma once
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/messaging/message.h"
#include <string>

namespace tbx
{
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

}
