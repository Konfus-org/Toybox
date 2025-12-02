#pragma once
#include "tbx/messages/message.h"
#include "tbx/messages/window_description.h"

namespace tbx
{
    class Window;

    struct TBX_API WindowOpenedEvent : public Event
    {
        WindowOpenedEvent(Window* window);

        Window* window = nullptr;
    };

    struct TBX_API WindowModeChangedEvent : public Event
    {
        WindowModeChangedEvent(WindowMode previous, WindowMode current);

        WindowMode previous = WindowMode::Windowed;
        WindowMode current = WindowMode::Windowed;
    };

    struct TBX_API WindowClosedEvent : public Event
    {
        WindowClosedEvent(Window* window);

        Window* window = nullptr;
    };

    struct TBX_API WindowDescriptionChangedEvent : public Event
    {
        WindowDescriptionChangedEvent(WindowDescription prev, WindowDescription curr);

        WindowDescription previous = {};
        WindowDescription current = {};
    };
}
