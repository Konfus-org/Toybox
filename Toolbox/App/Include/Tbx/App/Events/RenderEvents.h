#pragma once
#include <Tbx/Core/DllExport.h>
#include <Tbx/Core/Events/Event.h>
#include <string>

namespace Tbx
{
    class EXPORT RenderEvent : public Event
    {
    public:
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Render);
        }
    };

    class EXPORT BeginFrameEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "BeginFrameEvent";
        }
    };

    class EXPORT EndFrameEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "BeginFrameEvent";
        }
    };
}