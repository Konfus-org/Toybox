#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/messaging/observable.h"
#include "tbx/types/size.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines global graphics runtime settings shared across render plugins.
    /// @details
    /// Ownership: Owns all setting values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    struct TBX_API GraphicsSettings
    {
        GraphicsSettings(
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            uint32 shadow_map_resolution = 2048U,
            float shadow_render_distance = 90.0F,
            float shadow_softness = 1.0F,
            float local_light_max_distance = 64.0F,
            float shadow_caster_max_distance = 96.0F);

        GraphicsSettings(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            uint32 shadow_map_resolution = 2048U,
            float shadow_render_distance = 90.0F,
            float shadow_softness = 1.0F,
            float local_light_max_distance = 64.0F,
            float shadow_caster_max_distance = 96.0F);

        template <typename TOwner>
        GraphicsSettings(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            Observable<TOwner, GraphicsSettings>& parent,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            uint32 shadow_map_resolution = 2048U,
            float shadow_render_distance = 90.0F,
            float shadow_softness = 1.0F,
            float local_light_max_distance = 64.0F,
            float shadow_caster_max_distance = 96.0F)
            : vsync_enabled(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::vsync_enabled,
                  vsync ? VsyncMode::ON : VsyncMode::OFF)
            , graphics_api(dispatcher, parent, *this, &GraphicsSettings::graphics_api, api)
            , resolution(dispatcher, parent, *this, &GraphicsSettings::resolution, resolution)
            , shadow_map_resolution(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::shadow_map_resolution,
                  shadow_map_resolution)
            , shadow_render_distance(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::shadow_render_distance,
                  shadow_render_distance)
            , shadow_softness(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::shadow_softness,
                  shadow_softness)
            , local_light_max_distance(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::local_light_max_distance,
                  local_light_max_distance)
            , shadow_caster_max_distance(
                  dispatcher,
                  parent,
                  *this,
                  &GraphicsSettings::shadow_caster_max_distance,
                  shadow_caster_max_distance)
        {
        }

        /// @brief
        /// Purpose: Toggles presentation sync with the display refresh rate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, VsyncMode> vsync_enabled;

        /// @brief
        /// Purpose: Selects which graphics backend plugins should activate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, GraphicsApi> graphics_api;

        /// @brief
        /// Purpose: Sets the internal render resolution used by active renderers.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, Size> resolution;

        /// @brief
        /// Purpose: Sets square directional shadow-map texture resolution in pixels.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, uint32> shadow_map_resolution;

        /// @brief
        /// Purpose: Controls the camera-distance range covered by directional shadow cascades.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, float> shadow_render_distance;

        /// @brief
        /// Purpose: Controls directional shadow filter radius in shadow-map texels. Larger values
        /// produce softer edges while smaller values produce crisper edges.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, float> shadow_softness;

        /// @brief
        /// Purpose: Maximum distance from the active camera at which point, spot, and area lights
        /// are evaluated for scene lighting. Directional lights ignore this limit. Zero or
        /// negative values disable the limit (unbounded local lights).
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, float> local_light_max_distance;

        /// @brief
        /// Purpose: Maximum distance from the active camera at which opaque meshes may cast
        /// shadows for local lights and directional cascades. Zero or negative values disable the
        /// limit.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        Observable<GraphicsSettings, float> shadow_caster_max_distance;
    };

}
