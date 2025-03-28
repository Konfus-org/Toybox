#pragma once
#include <Tbx/Core/Events/Event.h>

namespace Tbx
{
    class EXPORT AppEvent : public Event
    {
    public:
        int GetCategorization() const override
        {
            return static_cast<int>(EventCategory::Application);
        }
    };

    class EXPORT AppInitializedEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Initialized Event";
        }
    };

    class EXPORT AppShutdownEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Started Event";
        }
    };

    class EXPORT AppUpdateEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Update Event";
        }
    };
}

