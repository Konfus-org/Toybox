#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Events/Event.h"

namespace Tbx
{
    class App;

    class TBX_EXPORT AppLaunchedEvent : public Event
    {
    public:
        AppLaunchedEvent(App* app)
        {
			_app = app;
        }

        std::string ToString() const override
        {
            return "App Initialized Event";
        }

        App& GetApp() const { return *_app; }

    private:
        App* _app = nullptr;
    };

    class TBX_EXPORT AppClosedEvent : public Event
    {
    public:
        AppClosedEvent(App* app)
        {
            _app = app;
        }

        std::string ToString() const override
        {
            return "App Closed Event";
        }

        App& GetApp() const { return *_app; }

    private:
        App* _app = nullptr;
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
        Settings _settings = {};
    };
}

