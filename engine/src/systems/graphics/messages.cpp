#include "tbx/systems/graphics/messages.h"

namespace tbx
{
    WindowOpenedEvent::WindowOpenedEvent(const Window& window_id)
        : window(window_id)
    {
    }

    WindowOpenedEvent::~WindowOpenedEvent() noexcept = default;

    WindowClosedEvent::WindowClosedEvent(const Window& window_id)
        : window(window_id)
    {
    }

    WindowClosedEvent::~WindowClosedEvent() noexcept = default;

    WindowTitleChangedEvent::WindowTitleChangedEvent(
        const Window& window_id,
        std::string previous_title,
        std::string current_title)
        : window(window_id)
        , previous(std::move(previous_title))
        , current(std::move(current_title))
    {
    }

    WindowTitleChangedEvent::~WindowTitleChangedEvent() noexcept = default;

    WindowSizeChangedEvent::WindowSizeChangedEvent(
        const Window& window_id,
        const Size& previous_size,
        const Size& current_size)
        : window(window_id)
        , previous(previous_size)
        , current(current_size)
    {
    }

    WindowSizeChangedEvent::~WindowSizeChangedEvent() noexcept = default;

    WindowModeChangedEvent::WindowModeChangedEvent(
        const Window& window_id,
        WindowMode previous_mode,
        WindowMode current_mode)
        : window(window_id)
        , previous(previous_mode)
        , current(current_mode)
    {
    }

    WindowModeChangedEvent::~WindowModeChangedEvent() noexcept = default;

    WindowNativeHandleChangedEvent::WindowNativeHandleChangedEvent(
        const Window& window_id,
        NativeWindowHandle previous_handle,
        NativeWindowHandle current_handle)
        : window(window_id)
        , previous(previous_handle)
        , current(current_handle)
    {
    }

    WindowNativeHandleChangedEvent::~WindowNativeHandleChangedEvent() noexcept = default;

}
