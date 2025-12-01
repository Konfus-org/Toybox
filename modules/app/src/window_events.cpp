#include "tbx/app/window_events.h"
#include <utility>

namespace tbx
{
    WindowOpenedEvent::WindowOpenedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }

    WindowModeChangedEvent::WindowModeChangedEvent(WindowMode previous, WindowMode current),
        previous_mode(previous), current_mode(current)
    {
    }

    WindowClosedEvent::WindowClosedEvent(Window* window_ptr)
        : window(window_ptr)
    {
    }
}
