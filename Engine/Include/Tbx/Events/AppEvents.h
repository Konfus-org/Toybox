#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Events/Event.h"

namespace Tbx
{
    class TBX_EXPORT AppInitializedEvent : public Event
    {
    public:
        std::string ToString() const override
        {
            return "App Initialized Event";
        }
    };

    class TBX_EXPORT AppExitingEvent : public Event
    {
    public:
        std::string ToString() const override
        {
            return "App Exiting Event";
        }
    };

    class TBX_EXPORT AppUpdatedEvent : public Event
    {
    public:
        std::string ToString() const override
        {
            return "App Update Event";
        }
    };

    class TBX_EXPORT AppSettingsChangedEvent : public Event
    {
    public:
        explicit AppSettingsChangedEvent(const Settings& settings)
            : _settings(settings) {}

        const Settings& GetNewSettings() const { return _settings; }

        std::string ToString() const override
        {
            return "App Settings Changed Event";
        }

    private:
        Settings _settings;
    };
}

