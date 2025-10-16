#pragma once
#include "Tbx/Windowing/Window.h"
#include "Tbx/Events/Event.h"
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
#include <string>

namespace Tbx
{
    struct TBX_EXPORT WindowActionEvent : public Event
    {
        explicit WindowActionEvent(const Window* window) : AffectedWindow(window) {}

        const Window* AffectedWindow = nullptr;
    };

    struct TBX_EXPORT WindowFocusedEvent final : public WindowActionEvent
    {
        using WindowActionEvent::WindowActionEvent;
    };

    struct TBX_EXPORT WindowOpenedEvent final : public WindowActionEvent
    {
        using WindowActionEvent::WindowActionEvent;
    };

    struct TBX_EXPORT WindowModeChangedEvent final : public WindowActionEvent
    {
        using WindowActionEvent::WindowActionEvent;
    };

    struct TBX_EXPORT WindowClosedEvent final : public WindowActionEvent
    {
        using WindowActionEvent::WindowActionEvent;
    };

    struct TBX_EXPORT WindowResizedEvent final : public WindowActionEvent
    {
        using WindowActionEvent::WindowActionEvent;
    };
}