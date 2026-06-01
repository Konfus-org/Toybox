#include "sdl_windowing_plugin.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_windowing
{
    void SdlWindowingPlugin::on_attach()
    {
        if (auto backend = window_backend.lock())
            backend->initialize();
        else
            TBX_TRACE_ERROR("SDL windowing plugin attached without a window backend service.");
    }

    void SdlWindowingPlugin::on_detach()
    {
        if (auto backend = window_backend.lock())
            backend->shutdown();
        window_backend = {};
    }
}
