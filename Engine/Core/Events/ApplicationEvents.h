#pragma once
#include "Math/Size.h"
#include "Event.h"

namespace Toybox
{
    class AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return EventCategory::Application;
        }
    };

    class AppUpdateEvent : public AppEvent 
    {
    public:
        std::string GetName() const override
        {
            return "App Update Event";
        }
    };

    class AppRenderEvent : public AppEvent 
    {
    public:
        std::string GetName() const override
        {
            return "App Render Event";
        }
    };
}

