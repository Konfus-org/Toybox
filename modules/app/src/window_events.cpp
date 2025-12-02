#include "tbx/app/window_events.h"
#include <utility>

namespace tbx
{
    WindowOpenedEvent::WindowOpenedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }

    WindowModeChangedEvent::WindowModeChangedEvent(WindowMode previous, WindowMode current)
        : previous(previous)
        , current(current)
    {
    }

    WindowClosedEvent::WindowClosedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }

    WindowDescriptionChangedEvent::WindowDescriptionChangedEvent(
        WindowDescription prev,
        WindowDescription curr)
        : previous(std::move(prev))
        , current(std::move(curr))
    {
    }
}
