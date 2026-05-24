#include "tbx/systems/app/messages.h"

namespace tbx
{
    ApplicationInitializedEvent::ApplicationInitializedEvent(Application& app)
        : application(app)
    {
    }

    ApplicationShutdownEvent::ApplicationShutdownEvent(Application& app)
        : application(app)
    {
    }

    ApplicationUpdateBeginEvent::ApplicationUpdateBeginEvent(Application& app, DeltaTime delta)
        : application(app)
        , delta_time(delta)
    {
    }

    ApplicationUpdateEndEvent::ApplicationUpdateEndEvent(Application& app, DeltaTime delta)
        : application(app)
        , delta_time(delta)
    {
    }
}
