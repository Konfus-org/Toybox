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

    class TBX_EXPORT WindowFocusedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const override
        {
            return "Window Focused Event";
        }
    };

    class TBX_EXPORT WindowOpenedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const override
        {
            return "Window Opened Event";
        }
    };

    class TBX_EXPORT WindowClosedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const override
        {
            return "Window Closed Event";
        }
    };

    class TBX_EXPORT WindowResizedEvent final : public WindowActionEvent
    {
    public:
        explicit WindowResizedEvent(Ref<Window> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const override
        {
            return "Window Resized Event";
        }
    };
}