#pragma once
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
        GraphicsSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0});

        Observable<GraphicsSettings, bool> vsync_enabled;
        Observable<GraphicsSettings, GraphicsApi> graphics_api;
        Observable<GraphicsSettings, Size> resolution;
    };
}
