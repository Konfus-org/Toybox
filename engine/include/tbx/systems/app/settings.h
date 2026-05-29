#pragma once
#include "tbx/systems/async/settings.h"
#include "tbx/systems/ecs/settings.h"
#include "tbx/systems/graphics/settings.h"
#include "tbx/systems/messaging/observable.h"
#include "tbx/systems/physics/settings.h"
#include "tbx/tbx_api.h"
#include "tbx/types/handle.h"
#include <filesystem>
#include <memory>

namespace tbx
{
    /// @brief
    /// Purpose: Stores file-system paths used by the application runtime.
    /// @details
    /// Ownership: Owns all stored path values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    struct TBX_API PathSettings
    {
        std::filesystem::path working_directory = {};
        std::filesystem::path logs_directory = {};
    };

    // TODO: Make app settings serializable and loadable like an asset.
    /// @brief
    /// Purpose: Stores mutable runtime settings for the application host.
    /// @details
    /// Ownership: Owns all stored settings values.
    /// Thread Safety: Not thread-safe; synchronize access externally.
    struct TBX_API AppSettings
    {
        AppSettings(
            std::weak_ptr<IMessageDispatcher> dispatcher,
            bool vsync = false,
            GraphicsApi api = GraphicsApi::OPEN_GL,
            Size resolution = {0, 0},
            AsyncSettings async_settings = {});

        Observable<AppSettings, GraphicsSettings> graphics;
        Observable<AppSettings, WorldSettings> world;
        PhysicsSettings physics;
        AsyncSettings async = {};
        PathSettings paths = {};

        // Startup icon asset used for native window icons.
        Handle icon = {};
    };
}
