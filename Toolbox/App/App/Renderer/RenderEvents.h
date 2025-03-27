#pragma once
#include <Core/ToolboxAPI.h>
#include <Core/Event Dispatcher/Event.h>

namespace Tbx
{
    class TBX_API RenderEvent : public Event
    {
    public:
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Render);
        }
    };

    class TBX_API BeginFrameEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "BeginFrameEvent";
        }
    };

    class TBX_API EndFrameEvent : public RenderEvent
    {
    public:
        std::string ToString() const final
        {
            return "BeginFrameEvent";
        }
    };
}