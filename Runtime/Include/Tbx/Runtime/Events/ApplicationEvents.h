#pragma once
#include "Tbx/Runtime/App/GraphicsSettings.h"
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

    class EXPORT AppGraphicsSettingsChangedEvent : public AppEvent
    {
    public:
        explicit AppGraphicsSettingsChangedEvent(const GraphicsSettings& settings)
            : _settings(settings) {}

        const GraphicsSettings& GetNewSettings() const { return _settings; }

        std::string ToString() const override
        {
            return "Set App Graphics Settings Event";
        }

    private:
        GraphicsSettings _settings;
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

