#include "tbx/app/app_events.h"

namespace tbx
{
    ApplicationInitializedEvent::ApplicationInitializedEvent(
        Application* app_ptr,
        const AppDescription& app_desc)
        : application(app_ptr)
        , description(app_desc)
    {
    }

    ApplicationShutdownEvent::ApplicationShutdownEvent(Application* app_ptr)
        : application(app_ptr)
    {
    }

    ApplicationUpdateBeginEvent::ApplicationUpdateBeginEvent(Application* app_ptr, DeltaTime delta)
        : application(app_ptr)
        , delta_time(delta)
    {
    }

    ApplicationUpdateEndEvent::ApplicationUpdateEndEvent(Application* app_ptr, DeltaTime delta)
        : application(app_ptr)
        , delta_time(delta)
    {
    }
}
