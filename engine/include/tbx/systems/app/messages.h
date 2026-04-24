#pragma once
#include "tbx/systems/app/application.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API ApplicationInitializedEvent : public Event
    {
        ApplicationInitializedEvent(Application& app);

        Application& application;
    };

    struct TBX_API ApplicationShutdownEvent : public Event
    {
        ApplicationShutdownEvent(Application& app);

        Application& application;
    };

    struct TBX_API ApplicationUpdateBeginEvent : public Event
    {
        ApplicationUpdateBeginEvent(Application& app, DeltaTime delta);

        Application& application;
        DeltaTime delta_time = {};
    };

    struct TBX_API ApplicationUpdateEndEvent : public Event
    {
        ApplicationUpdateEndEvent(Application& app, DeltaTime delta);

        Application& application;
        DeltaTime delta_time = {};
    };

    struct TBX_API ExitApplicationRequest : public Request<void>
    {
    };
}
