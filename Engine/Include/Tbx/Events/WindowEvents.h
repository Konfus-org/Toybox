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
        explicit WindowActionEvent(const Window* window) : _window(window) {}

        const Window* GetWindow() const { return _window; }

    private:
        const Window* _window = nullptr;
    };

    class TBX_EXPORT WindowFocusedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(const Window* window)
            : WindowActionEvent(window) {}

        std::string ToString() const override
        {
            return "Window Focused Event";
        }
    };

    class TBX_EXPORT WindowOpenedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(const Window* window)
            : WindowActionEvent(window) {}

        std::string ToString() const override
        {
            return "Window Opened Event";
        }
    };

    class TBX_EXPORT WindowClosedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(const Window* window)
            : WindowActionEvent(window) {}

        std::string ToString() const override
        {
            return "Window Closed Event";
        }
    };

    class TBX_EXPORT WindowResizedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowResizedEvent(const Window* window)
            : WindowActionEvent(window) {}

        std::string ToString() const override
        {
            return "Window Resized Event";
        }
    };
}