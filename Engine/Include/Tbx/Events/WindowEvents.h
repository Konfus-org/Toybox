#pragma once
#include "Tbx/Windowing/Window.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
#include <string>

namespace Tbx
{
    class TBX_EXPORT WindowActionEvent : public Event
    {
    public:
        explicit WindowActionEvent(Ref<Window> window) : _window(std::move(window)) {}

        Ref<Window> GetWindow() const { return _window; }

    private:
        Ref<Window> _window = nullptr;
    };

    class TBX_EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class TBX_EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class TBX_EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };

    class TBX_EXPORT WindowResizedEvent : public WindowActionEvent
    {
    public:
        explicit WindowResizedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Resized Event";
        }
    };
}