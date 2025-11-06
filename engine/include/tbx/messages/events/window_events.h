#pragma once
#include "tbx/messages/events/event.h"
#include "tbx/os/window.h"

namespace tbx
{
    class TBX_API WindowOpenedEvent : public Event
    {
      public:
        WindowOpenedEvent(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window that opened.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    class TBX_API WindowModeChangedEvent : public Event
    {
      public:
        WindowModeChangedEvent(Window* window_ptr, WindowMode previous, WindowMode current);

        // Non-owning pointer to the window whose mode changed.
        Window* window = nullptr;
        WindowMode previous_mode = WindowMode::Windowed;
        WindowMode current_mode = WindowMode::Windowed;
    };

    class TBX_API WindowClosedEvent : public Event
    {
      public:
        explicit WindowClosedEvent(Window* window_ptr);

        // Non-owning pointer to the window that closed.
        Window* window = nullptr;
    };
}
