#pragma once
#include "tbx/app/app_description.h"
#include "tbx/messages/message.h"
#include "tbx/time/delta_time.h"

namespace tbx
{
    class Application;

    struct TBX_API ApplicationInitializedEvent : public Event
    {
        ApplicationInitializedEvent(Application* app_ptr);

        Application* application = nullptr;
    };

    struct TBX_API ApplicationShutdownEvent : public Event
    {
        ApplicationShutdownEvent(Application* app_ptr);

        Application* application = nullptr;
    };

    struct TBX_API ApplicationUpdateBeginEvent : public Event
    {
        ApplicationUpdateBeginEvent(Application* app_ptr, DeltaTime delta);

        Application* application = nullptr;
        DeltaTime delta_time = {};
    };

    struct TBX_API ApplicationUpdateEndEvent : public Event
    {
        ApplicationUpdateEndEvent(Application* app_ptr, DeltaTime delta);

        Application* application = nullptr;
        DeltaTime delta_time = {};
    };
}
