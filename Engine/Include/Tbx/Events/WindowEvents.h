#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Ids/UID.h"
#include "Tbx/Math/Size.h"
#include <string>
#include <memory>

namespace Tbx
{
    class EXPORT WindowEvent : public Event
    {
    public:
        int GetCategorization() const final
        { 
            return static_cast<int>(EventCategory::Window);
        }
    };

    class EXPORT WindowActionEvent : public WindowEvent
    {
    public:
        explicit WindowActionEvent(UID windowId) : _windowId(windowId) {}

        UID GetWindowId() const { return _windowId; }

    private:
        UID _windowId = -1;
    };

    class EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(UID windowId)
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(UID windowId)
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(UID windowId) 
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };

    class EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        WindowResizedEvent(UID windowId, uint width, uint height)
            : WindowActionEvent(windowId), _width(width), _height(height) {}

        Size GetNewSize() const
        {
            return Size(_width, _height);
        }

        std::string ToString() const final
        {
            return "Window Resize Event";
        }

    private:
        uint _width;
        uint _height;
    };
}