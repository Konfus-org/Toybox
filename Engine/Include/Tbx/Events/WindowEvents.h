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
        explicit WindowActionEvent(Uid windowId) : _windowId(windowId) {}

        Uid GetWindowId() const { return _windowId; }

    private:
        Uid _windowId = Invalid::Uid;
    };

    class EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(Uid windowId)
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(Uid windowId)
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(Uid windowId) 
            : WindowActionEvent(windowId) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };

    class EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        WindowResizedEvent(Uid windowId, Size newSize)
            : WindowActionEvent(windowId), _newSize(newSize) {}

        const Size& GetNewSize() const
        {
            return _newSize;
        }

        std::string ToString() const final
        {
            return "Window Resize Event";
        }

    private:
        Size _newSize;
    };
}