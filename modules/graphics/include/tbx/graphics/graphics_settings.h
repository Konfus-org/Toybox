#pragma once
#include "tbx/common/int.h"
#include "tbx/graphics/graphics_api.h"
#include "tbx/math/size.h"
#include "tbx/messages/observable.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    /// @brief
    /// Purpose: Enumerates render stage outputs that can be presented to the swapchain.
    /// @details
    /// Ownership: Enum values are copied by value by configuration systems.
    /// Thread Safety: Thread-safe as immutable enum constants; consuming settings remain externally
    /// synchronized.

    enum class RenderStage
    {
        FINAL_COLOR,
        GEOMETRY_PREVIEW_COLOR,
        ALBEDO,
        NORMAL,
        DEPTH_PREVIEW,
    };

    /// @brief
    /// Purpose: Defines global graphics runtime settings shared across render plugins.
    /// @details
    /// Ownership: Owns all setting values by value.
    /// Thread Safety: Not thread-safe; synchronize access externally.

    struct TBX_API GraphicsSettings
    {
        GraphicsSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            RenderStage render_stage = RenderStage::FINAL_COLOR,
            uint32 shadow_map_resolution = 2048U,
            float shadow_render_distance = 90.0F,
            float shadow_softness = 1.0F);

        /// @brief
        /// Purpose: Toggles presentation sync with the display refresh rate.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        Observable<GraphicsSettings, bool> vsync_enabled;

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
        /// Purpose: Selects which render stage output is presented to the backbuffer.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        Observable<GraphicsSettings, RenderStage> render_stage;

        /// @brief
        /// Purpose: Sets square directional shadow-map texture resolution in pixels.
        /// @details
        /// Ownership: Value owned by this settings object.
        /// Thread Safety: Not thread-safe; synchronize access externally.

        Observable<GraphicsSettings, uint32> shadow_map_resolution;

        /// @brief
        /// Purpose: Controls how far directional shadows are rendered from the camera. The OpenGL
        /// backend uses this as the shadow far plane while keeping a fixed near plane.
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
    };
}
