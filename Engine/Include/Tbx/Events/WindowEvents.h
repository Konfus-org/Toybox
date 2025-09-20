#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Size.h"
#include "Tbx/Memory/Refs.h"
#include <string>
#include <memory>

namespace Tbx
{
    class EXPORT WindowActionEvent : public Event
    {
    public:
        explicit WindowActionEvent(Ref<IWindow> window) : _window(std::move(window)) {}

        Ref<IWindow> GetWindow() const { return _window; }

    private:
        Ref<IWindow> _window = nullptr;
    };

    class EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };
}