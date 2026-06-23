#pragma once
#include "tbx/systems/graphics/api.h"
#include "tbx/systems/graphics/settings.generated.h"
#include "tbx/tbx_api.h"
#include "tbx/types/clamp.h"
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
        /// Purpose: Sets the render resolution used by active renderers — the game window's size and
        /// the editor's view textures both follow this.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        Size resolution = {1280, 720};

        /// @brief
        /// Purpose: Sets the square directional shadow-map texture resolution in pixels for the
        /// nearest (cascade 0) shadow map. Each further cascade halves this (floored at 256), so the
        /// furthest cascade is deliberately low resolution since it spreads over the whole far range.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        Clamp<uint32, 256U> shadow_map_resolution = 4096U;

        /// @brief
        /// Purpose: Controls how far directional shadows reach. The shadow frustum is split into
        /// distance cascades from the camera out to this distance; the nearest cascade is the sharpest
        /// and the furthest reaches this far at low resolution. Larger values reach farther but spread
        /// each cascade's texels over more world, so raise shadow_map_resolution to compensate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float shadow_render_distance = 500.0F;

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
        /// are evaluated for scene lighting. Directional lights ignore this limit. Approaching this
        /// distance a local light fades its intensity to zero (over the outer ~20%) so it dims away
        /// smoothly instead of blipping out. Zero or negative values disable the limit (unbounded
        /// local lights).
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.
        [[prop]]
        float local_light_max_distance = 200.0F;

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
