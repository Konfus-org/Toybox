#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Events/Event.h"

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

    class EXPORT AppSettingsChangedEvent : public AppEvent
    {
    public:
        explicit AppSettingsChangedEvent(const Settings& settings)
            : _settings(settings) {}

        const Settings& GetNewSettings() const { return _settings; }

        std::string ToString() const override
        {
            return "Set App Settings Event";
        }

    private:
        Settings _settings;
    };

    class EXPORT AppShutdownEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Started Event";
        }
    };

    class EXPORT AppUpdatedEvent : public AppEvent
    {
    public:
        std::string ToString() const override
        {
            return "App Update Event";
        }
    };
}

