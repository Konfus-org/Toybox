#pragma once
#include "Event.h"
#include "TbxAPI.h"

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