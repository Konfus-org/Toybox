#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/Status.h"
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

    class TBX_EXPORT AppStatusChangedEvent : public Event
    {
    public:
        explicit AppStatusChangedEvent(const AppStatus& status)
            : _status(status) {
        }

        const AppStatus& GetNewStatus() const { return _status; }

        std::string ToString() const override
        {
            return "App Settings Changed Event";
        }

    private:
        AppStatus _status = {};
    };

    class TBX_EXPORT AppSettingsChangedEvent : public Event
    {
    public:
        explicit AppSettingsChangedEvent(const AppSettings& settings)
            : _settings(settings) {}

        const AppSettings& GetNewSettings() const { return _settings; }

        std::string ToString() const override
        {
            return "App Settings Changed Event";
        }

    private:
        AppSettings _settings = {};
    };
}

