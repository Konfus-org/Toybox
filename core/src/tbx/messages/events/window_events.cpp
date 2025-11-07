#include "tbx/messages/events/window_events.h"
#include <utility>

namespace tbx
{
    WindowOpenedEvent::WindowOpenedEvent(Window* window_ptr, WindowDescription desc)
        : window(window_ptr)
        , description(std::move(desc))
    {
    }

    WindowModeChangedEvent::WindowModeChangedEvent(
        Window* window_ptr,
        WindowMode previous,
        WindowMode current)
        : window(window_ptr)
        , previous_mode(previous)
        , current_mode(current)
    {
    }

    WindowClosedEvent::WindowClosedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }
}
