#pragma once
#include "Math/Size.h"
#include "Event.h"
#include "TbxAPI.h"

namespace Tbx
{
    class TBX_API AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return static_cast<int>(EventCategory::Application);
        }
    };

    class TBX_API AppUpdateEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Update Event";
        }
    };

    class TBX_API AppRenderEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Render Event";
        }
    };
}

