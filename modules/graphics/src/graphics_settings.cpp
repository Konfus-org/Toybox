#include "tbx/graphics/graphics_settings.h"

namespace tbx
{
    GraphicsSettings::GraphicsSettings(
        IMessageDispatcher& dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution)
        : vsync_enabled(&dispatcher, this, &GraphicsSettings::vsync_enabled, vsync)
        , graphics_api(&dispatcher, this, &GraphicsSettings::graphics_api, api)
        , resolution(&dispatcher, this, &GraphicsSettings::resolution, resolution)
    {
    }
}
