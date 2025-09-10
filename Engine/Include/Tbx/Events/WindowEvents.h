#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
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

    class EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        WindowResizedEvent(std::shared_ptr<IWindow> window, Size newSize)
            : WindowActionEvent(std::move(window)), _newSize(newSize) {}

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