#include "sdl_windowing_plugin.h"
#include "tbx/systems/debugging/macros.h"

namespace sdl_windowing
{
    void SdlWindowing::on_attach()
    {
        if (auto backend = window_backend.lock())
            backend->initialize();
        else
            TBX_TRACE_ERROR("SDL windowing plugin attached without a window backend service.");
    }

    void SdlWindowing::on_detach()
    {
        if (auto backend = window_backend.lock())
            backend->shutdown();
        window_backend = {};
    }
}
