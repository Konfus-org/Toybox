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

    class TBX_API AppInitializedEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Initialized Event";
        }
    };

    class TBX_API AppShutdownEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Started Event";
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
}

