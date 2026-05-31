#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/graphics/settings.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/size.h"

namespace tbx
{
    /// @brief
    /// Purpose: Defines global graphics runtime settings shared across render plugins.
    /// @details
    /// Ownership: Owns all setting values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    [[serializable]];
    struct TBX_API GraphicsSettings
    {
        /// @brief
        /// Purpose: Toggles presentation sync with the display refresh rate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        VsyncMode vsync_enabled = VsyncMode::OFF;

        /// @brief
        /// Purpose: Selects which graphics backend plugins should activate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        GraphicsApi graphics_api = GraphicsApi::OPEN_GL;

        /// @brief
        /// Purpose: Sets the internal render resolution used by active renderers.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        Size resolution = {0, 0};

        /// @brief
        /// Purpose: Sets square directional shadow-map texture resolution in pixels.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        uint32 shadow_map_resolution = 2048U;

        /// @brief
        /// Purpose: Controls the camera-distance range covered by directional shadow cascades.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float shadow_render_distance = 90.0F;

        /// @brief
        /// Purpose: Controls directional shadow filter radius in shadow-map texels. Larger values
        /// produce softer edges while smaller values produce crisper edges.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float shadow_softness = 1.0F;

        /// @brief
        /// Purpose: Maximum distance from the active camera at which point, spot, and area lights
        /// are evaluated for scene lighting. Directional lights ignore this limit. Zero or
        /// negative values disable the limit (unbounded local lights).
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float local_light_max_distance = 64.0F;

        /// @brief
        /// Purpose: Maximum distance from the active camera at which opaque meshes may cast
        /// shadows for local lights and directional cascades. Zero or negative values disable the
        /// limit.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float shadow_caster_max_distance = 96.0F;
    };

}
