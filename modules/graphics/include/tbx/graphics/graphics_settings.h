#pragma once
#include "tbx/common/int.h"
#include "tbx/graphics/graphics_api.h"
#include "tbx/math/size.h"
#include "tbx/messages/observable.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// <summary>
    /// Purpose: Defines global graphics runtime settings shared across render plugins.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all setting values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    /// </remarks>
    struct TBX_API GraphicsSettings
    {
        /// <summary>
        /// Purpose: Initializes graphics settings with observable defaults.
        /// </summary>
        /// <remarks>
        /// Ownership: Stores all setting values by copy inside this struct.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </remarks>
        GraphicsSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            uint32 shadow_map_resolution = 2048U,
            float shadow_render_distance = 90.0F,
            float shadow_softness = 1.0F);

        /// <summary>
        /// Purpose: Toggles presentation sync with the display refresh rate.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, bool> vsync_enabled;

        /// <summary>
        /// Purpose: Selects which graphics backend plugins should activate.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, GraphicsApi> graphics_api;

        /// <summary>
        /// Purpose: Sets the internal render resolution used by active renderers.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, Size> resolution;

        /// <summary>
        /// Purpose: Sets square directional shadow-map texture resolution in pixels.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, uint32> shadow_map_resolution;

        /// <summary>
        /// Purpose: Controls how far directional shadows are rendered from the camera.
        /// The OpenGL backend uses this as the shadow far plane while keeping a fixed near plane.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, float> shadow_render_distance;

        /// <summary>
        /// Purpose: Controls directional shadow filter radius in shadow-map texels.
        /// Larger values produce softer edges while smaller values produce crisper edges.
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        /// </summary>
        Observable<GraphicsSettings, float> shadow_softness;
    };
}
