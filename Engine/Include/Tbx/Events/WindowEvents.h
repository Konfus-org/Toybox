#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Size.h"
#include <string>
#include <memory>

namespace Tbx
{
    class EXPORT WindowActionEvent : public Event
    {
    public:
        explicit WindowActionEvent(std::shared_ptr<IWindow> window) : _window(std::move(window)) {}

        std::shared_ptr<IWindow> GetWindow() const { return _window; }

    private:
        std::shared_ptr<IWindow> _window = nullptr;
    };

    class EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(std::shared_ptr<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(std::shared_ptr<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(std::shared_ptr<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };
}