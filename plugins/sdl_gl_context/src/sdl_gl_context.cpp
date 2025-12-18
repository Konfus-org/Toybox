#include "sdl_adapter_plugin.h"
#include "tbx/debugging/macros.h"
#include <SDL3/SDL.h>

namespace tbx::plugins::sdladapter
{
    void SdlGlContextPlugin::on_attach(Application&) {}

    void SdlGlContextPlugin::on_detach() {}

    void SdlGlContextPlugin::on_update(const DeltaTime&) {}
}
