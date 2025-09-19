#pragma once
#include "Tbx/Windowing/IWindow.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Math/Size.h"
#include <string>
#include <memory>
#include "Tbx/TypeAliases/Pointers.h"

namespace Tbx
{
    class EXPORT WindowActionEvent : public Event
    {
    public:
        explicit WindowActionEvent(Tbx::Ref<IWindow> window) : _window(std::move(window)) {}

        Tbx::Ref<IWindow> GetWindow() const { return _window; }

    private:
        Tbx::Ref<IWindow> _window = nullptr;
    };

    class EXPORT WindowFocusedEvent : public WindowActionEvent
    {
    public:
        explicit WindowFocusedEvent(Tbx::Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Focused Event";
        }
    };

    class EXPORT WindowOpenedEvent : public WindowActionEvent
    {
    public:
        explicit WindowOpenedEvent(Tbx::Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Opened Event";
        }
    };

    class EXPORT WindowClosedEvent : public WindowActionEvent
    {
    public:
        explicit WindowClosedEvent(Tbx::Ref<IWindow> window)
            : WindowActionEvent(std::move(window)) {}

        std::string ToString() const final
        {
            return "Window Closed Event";
        }
    };
}