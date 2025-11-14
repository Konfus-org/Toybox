#pragma once
#include "tbx/app/app_description.h"
#include "tbx/messages/events/event.h"
#include "tbx/time/delta_time.h"

namespace tbx
{
    class Application;

    struct TBX_API ApplicationInitializedEvent : public Event
    {
        ApplicationInitializedEvent(Application* app_ptr, const AppDescription& app_desc);

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        AppDescription description = {};
    };

    struct TBX_API ApplicationShutdownEvent : public Event
    {
        ApplicationShutdownEvent(Application* app_ptr);

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
    };

    struct TBX_API ApplicationUpdateBeginEvent : public Event
    {
        ApplicationUpdateBeginEvent(Application* app_ptr, DeltaTime delta);

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        DeltaTime delta_time = {};
    };

    struct TBX_API ApplicationUpdateEndEvent : public Event
    {
        ApplicationUpdateEndEvent(Application* app_ptr, DeltaTime delta);

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        DeltaTime delta_time = {};
    };
}
