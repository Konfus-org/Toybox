#pragma once
#include "tbx/messages/message.h"
#include "tbx/messages/window_description.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class Window;

    // Request requesting a new platform window.
    struct TBX_API CreateWindowRequest : public Request<std::any>
    {
        CreateWindowRequest() = default;
        CreateWindowRequest(WindowDescription desc);

        WindowDescription description = {};
    };

    struct TBX_API WindowOpenedEvent : public Event
    {
        WindowOpenedEvent() = default;
        WindowOpenedEvent(Window* window_ptr);

        // Non-owning pointer to the window that was opened.
        Window* window = nullptr;
    };

    struct TBX_API QueryWindowDescriptionRequest : public Request<WindowDescription>
    {
        QueryWindowDescriptionRequest() = default;
        QueryWindowDescriptionRequest(Window* window_ptr);

        // Non-owning pointer to the window being queried.
        Window* window = nullptr;
    };

    struct TBX_API WindowDescriptionChangedEvent : public Event
    {
        WindowDescriptionChangedEvent() = default;
        WindowDescriptionChangedEvent(WindowDescription prev, WindowDescription curr);

        // Non-owning pointer to the window to be updated.
        WindowDescription previous = {};
        WindowDescription current = {};
    };

    struct TBX_API WindowClosedEvent : public Event
    {
        WindowClosedEvent() = default;
        WindowClosedEvent(Window* window_ptr);

        // Non-owning pointer to the window being closed.
        Window* window = nullptr;
    };
}
