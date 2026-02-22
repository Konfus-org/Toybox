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
        /// <summary>
        /// Purpose: Constructs observable graphics settings with optional initial values.
        /// </summary>
        /// <remarks>
        /// Ownership: Does not assume ownership of the dispatcher reference.
        /// Thread Safety: Not thread-safe; initialize on one thread.
        /// </remarks>
        GraphicsSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0});

        Observable<GraphicsSettings, bool> vsync_enabled;
        Observable<GraphicsSettings, GraphicsApi> graphics_api;
        Observable<GraphicsSettings, Size> resolution;

        /// <summary>
        /// Purpose: Validates that graphics settings form a usable render configuration.
        /// </summary>
        /// <remarks>
        /// Ownership: Reads internal values only and does not transfer ownership.
        /// Thread Safety: Safe for concurrent reads when no concurrent mutation occurs.
        /// </remarks>
        bool get_is_valid() const;
    };
}
