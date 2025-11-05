#pragma once
#include "tbx/app_description.h"

namespace tbx
{
    class Application;

    struct TBX_API ApplicationContext
    {
        // Non-owning pointer to the host application instance.
        // Lifetime: valid only for the duration of `on_attach`/`on_update`/`on_detach`
        // on the same thread managed by the host.
        Application* instance = nullptr;

        // Snapshot of the application description; owned by the context value.
        AppDescription description = {};
    };
}
