#include "tbx/systems/graphics/settings.h"

namespace tbx
{
    GraphicsSettings::GraphicsSettings(
        detail::GraphicsSettingsLocalCopyTag,
        bool vsync,
        GraphicsApi api,
        Size resolution,
        uint32 shadow_map_resolution,
        float shadow_render_distance,
        float shadow_softness,
        float local_light_max_distance,
        float shadow_caster_max_distance)
        : vsync_enabled(*this, &GraphicsSettings::vsync_enabled, vsync)
        , graphics_api(*this, &GraphicsSettings::graphics_api, api)
        , resolution(*this, &GraphicsSettings::resolution, resolution)
        , shadow_map_resolution(
              *this,
              &GraphicsSettings::shadow_map_resolution,
              shadow_map_resolution)
        , shadow_render_distance(
              *this,
              &GraphicsSettings::shadow_render_distance,
              shadow_render_distance)
        , shadow_softness(*this, &GraphicsSettings::shadow_softness, shadow_softness)
        , local_light_max_distance(
              *this,
              &GraphicsSettings::local_light_max_distance,
              local_light_max_distance)
        , shadow_caster_max_distance(
              *this,
              &GraphicsSettings::shadow_caster_max_distance,
              shadow_caster_max_distance)
    {
    }

    GraphicsSettings::GraphicsSettings(
        IMessageDispatcher& dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution,
        uint32 shadow_map_resolution,
        float shadow_render_distance,
        float shadow_softness,
        float local_light_max_distance,
        float shadow_caster_max_distance)
        : vsync_enabled(dispatcher, *this, &GraphicsSettings::vsync_enabled, vsync)
        , graphics_api(dispatcher, *this, &GraphicsSettings::graphics_api, api)
        , resolution(dispatcher, *this, &GraphicsSettings::resolution, resolution)
        , shadow_map_resolution(
              dispatcher,
              *this,
              &GraphicsSettings::shadow_map_resolution,
              shadow_map_resolution)
        , shadow_render_distance(
              dispatcher,
              *this,
              &GraphicsSettings::shadow_render_distance,
              shadow_render_distance)
        , shadow_softness(dispatcher, *this, &GraphicsSettings::shadow_softness, shadow_softness)
        , local_light_max_distance(
              dispatcher,
              *this,
              &GraphicsSettings::local_light_max_distance,
              local_light_max_distance)
        , shadow_caster_max_distance(
              dispatcher,
              *this,
              &GraphicsSettings::shadow_caster_max_distance,
              shadow_caster_max_distance)
    {
    }

    GraphicsSettings::GraphicsSettings(const GraphicsSettings& other)
        : GraphicsSettings(
              detail::GraphicsSettingsLocalCopyTag {},
              other.vsync_enabled,
              other.graphics_api,
              other.resolution,
              other.shadow_map_resolution,
              other.shadow_render_distance,
              other.shadow_softness,
              other.local_light_max_distance,
              other.shadow_caster_max_distance)
    {
    }

    GraphicsSettings& GraphicsSettings::operator=(const GraphicsSettings& other)
    {
        if (this == &other)
            return *this;

        vsync_enabled = other.vsync_enabled.value;
        graphics_api = other.graphics_api.value;
        resolution = other.resolution.value;
        shadow_map_resolution = other.shadow_map_resolution.value;
        shadow_render_distance = other.shadow_render_distance.value;
        shadow_softness = other.shadow_softness.value;
        local_light_max_distance = other.local_light_max_distance.value;
        shadow_caster_max_distance = other.shadow_caster_max_distance.value;
        return *this;
    }

    GraphicsSettings::GraphicsSettings(GraphicsSettings&& other) noexcept
        : GraphicsSettings(other)
    {
    }

    GraphicsSettings& GraphicsSettings::operator=(GraphicsSettings&& other) noexcept
    {
        return operator=(other);
    }
}
