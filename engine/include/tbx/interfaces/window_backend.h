#pragma once
#include "tbx/tbx_api.h"
#include "tbx/types/window.h"
#include <string>
#include <vector>

namespace tbx
{
    enum class WindowBackendEventType
    {
        CLOSE_REQUESTED,
        RESIZED,
        MINIMIZED,
        RESTORED,
        QUIT_REQUESTED
    };

    struct TBX_API WindowBackendEvent
    {
        WindowBackendEventType type = WindowBackendEventType::CLOSE_REQUESTED;
        Window window = {};
        Size size = {};
    };

    /// @brief
    /// Purpose: Backend interface implemented by platform-specific windowing plugins.
    /// @details
    /// Ownership: Implementations own native window resources and return non-owning native handles
    /// to callers.
    /// Thread Safety: Not thread-safe unless a backend explicitly documents otherwise.
    class TBX_API IWindowBackend
    {
      public:
        virtual ~IWindowBackend() noexcept = default;

      public:
        virtual void initialize() = 0;
        virtual void shutdown() = 0;

        virtual void pump_events(std::vector<WindowBackendEvent>& out_events) = 0;

        virtual bool create_window(
            const Window& window,
            const WindowCreateInfo& create_info,
            NativeWindowHandle& out_native_handle) = 0;
        virtual bool destroy_window(const Window& window) = 0;

        virtual bool set_window_mode(const Window& window, WindowMode mode) = 0;
        virtual bool set_window_title(const Window& window, const std::string& title) = 0;
        virtual bool set_window_size(const Window& window, const Size& size) = 0;
    };
}
