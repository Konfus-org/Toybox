#pragma once
#include "tbx/systems/app/application.h"
#include "tbx/systems/messaging/message.h"
#include "tbx/systems/time/delta_time.h"

namespace tbx
{
    struct TBX_API ApplicationInitializedEvent : public Event
    {
        explicit ApplicationInitializedEvent(Application& app);

        Application& application;
    };

    struct TBX_API ApplicationShutdownEvent : public Event
    {
        explicit ApplicationShutdownEvent(Application& app);

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

    /// @brief
    /// Purpose: Pauses or resumes simulation (world, scripts, fixed updates) while rendering and
    /// plugins keep running, so external tools can freeze gameplay without losing their view.
    struct TBX_API SetApplicationPausedRequest : public Request<void>
    {
        explicit SetApplicationPausedRequest(bool paused)
            : is_paused(paused)
        {
        }

        bool is_paused = false;
    };
}
