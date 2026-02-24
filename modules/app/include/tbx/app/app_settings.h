#pragma once
#include "tbx/async/async_settings.h"
#include "tbx/graphics/graphics_settings.h"
#include "tbx/physics/physics_settings.h"
#include "tbx/tbx_api.h"
#include <filesystem>

namespace tbx
{
    /// <summary>
    /// Purpose: Stores file-system paths used by the application runtime.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all stored path values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    /// </remarks>
    struct TBX_API PathSettings
    {
        std::filesystem::path working_directory = {};
        std::filesystem::path logs_directory = {};
    };

    /// <summary>
    /// Purpose: Stores mutable runtime settings for the application host.
    /// </summary>
    /// <remarks>
    /// Ownership: Owns all stored settings values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    /// </remarks>
    struct TBX_API AppSettings
    {
        AppSettings(
            IMessageDispatcher& dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            AsyncSettings async_settings = {});

        GraphicsSettings graphics;
        PhysicsSettings physics;
        AsyncSettings async = {};
        PathSettings paths = {};
    };
}
