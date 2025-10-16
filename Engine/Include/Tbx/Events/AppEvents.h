#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/Status.h"
#include "Tbx/Events/Event.h"

namespace Tbx
{
    class App;

    struct TBX_EXPORT AppLaunchedEvent : public Event
    {
        AppLaunchedEvent(const App* app) : LaunchedApp(app) {}

        const App* LaunchedApp = nullptr;
    };

    struct TBX_EXPORT AppClosedEvent : public Event
    {
        AppClosedEvent(const App* app) : ClosedApp(app) {}

        const App* ClosedApp = nullptr;
    };

    struct TBX_EXPORT AppUpdatedEvent : public Event
    {
        AppUpdatedEvent(const App* app) : UpdatedApp(app) {}

        const App* UpdatedApp = nullptr;
    };

    struct TBX_EXPORT AppStatusChangedEvent : public Event
    {
        AppStatusChangedEvent(const AppStatus& oldStatus, const AppStatus& newStatus)
            : OldStatus(oldStatus), NewStatus(newStatus) {}

        const AppStatus OldStatus = {};
        const AppStatus NewStatus = {};
    };

    struct TBX_EXPORT AppSettingsChangedEvent : public Event
    {
        AppSettingsChangedEvent(const AppSettings& oldSettings, const AppSettings& newSettings) 
            : OldSettings(oldSettings), NewSettings(newSettings) {}

        const AppSettings OldSettings = {};
        const AppSettings NewSettings = {};
    };
}

