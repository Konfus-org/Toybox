#pragma once
#include "Math/Size.h"
#include "Event.h"
#include "tbxAPI.h"

namespace Toybox
{
    class TBX_API AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return EventCategory::Application;
        }
    };

    class TBX_API AppUpdateEvent : public AppEvent
    {
    public:
        std::string GetName() const override
        {
            return "App Update Event";
        }
    };

    class TBX_API AppRenderEvent : public AppEvent
    {
    public:
        std::string GetName() const override
        {
            return "App Render Event";
        }
    };
}

