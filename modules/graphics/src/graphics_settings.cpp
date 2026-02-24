#include "tbx/graphics/graphics_settings.h"

namespace tbx
{
    GraphicsSettings::GraphicsSettings(
        IMessageDispatcher& dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution,
        uint32 shadow_map_resolution,
        float shadow_render_distance,
        float shadow_softness)
        : vsync_enabled(&dispatcher, this, &GraphicsSettings::vsync_enabled, vsync)
        , graphics_api(&dispatcher, this, &GraphicsSettings::graphics_api, api)
        , resolution(&dispatcher, this, &GraphicsSettings::resolution, resolution)
        , shadow_map_resolution(
              &dispatcher,
              this,
              &GraphicsSettings::shadow_map_resolution,
              shadow_map_resolution)
        , shadow_render_distance(
              &dispatcher,
              this,
              &GraphicsSettings::shadow_render_distance,
              shadow_render_distance)
        , shadow_softness(&dispatcher, this, &GraphicsSettings::shadow_softness, shadow_softness)
    {
    }
}
