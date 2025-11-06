#pragma once
#include "tbx/app_description.h"
#include "tbx/messages/events/event.h"
#include "tbx/time/delta_time.h"

namespace tbx
{
    class Application;

    class TBX_API ApplicationInitializedEvent : public Event
    {
      public:
        ApplicationInitializedEvent(Application* app_ptr, const AppDescription& app_desc)
            : application(app_ptr)
            , description(app_desc)
        {
        }

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        AppDescription description = {};
    };

    class TBX_API ApplicationShutdownEvent : public Event
    {
      public:
        explicit ApplicationShutdownEvent(Application* app_ptr)
            : application(app_ptr)
        {
        }

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
    };

    class TBX_API ApplicationUpdateBeginEvent : public Event
    {
      public:
        ApplicationUpdateBeginEvent(Application* app_ptr, DeltaTime delta)
            : application(app_ptr)
            , delta_time(delta)
        {
        }

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        DeltaTime delta_time = {};
    };

    class TBX_API ApplicationUpdateEndEvent : public Event
    {
      public:
        ApplicationUpdateEndEvent(Application* app_ptr, DeltaTime delta)
            : application(app_ptr)
            , delta_time(delta)
        {
        }

        // Non-owning pointer to the application that emitted the event.
        Application* application = nullptr;
        DeltaTime delta_time = {};
    };
}
