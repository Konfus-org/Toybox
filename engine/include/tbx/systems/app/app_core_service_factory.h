#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/app/app_service_provider.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/world/manager.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <memory>
#include <vector>

namespace tbx
{
    /// @brief
    /// Purpose: The core services the application caches as weak members for per-frame and
    /// teardown use. Services built only for registration (serialization registry, job system) are
    /// not returned because the application never holds a direct handle to them.
    struct AppCoreServices
    {
        std::shared_ptr<IFileOps> file_ops = {};
        std::shared_ptr<IMessageCoordinator> message_coordinator = {};
        std::shared_ptr<AssetManager> asset_manager = {};
        std::shared_ptr<WorldManager> world_manager = {};
        std::shared_ptr<ThreadManager> thread_manager = {};
        std::shared_ptr<ScriptSystem> script_system = {};
    };

    /// @brief
    /// Purpose: Builds the engine-owned core services (file/message/serialization/assets/world/
    /// script/job/thread) on a service provider during startup.
    /// @details
    /// Ownership: Borrows the provider it is constructed with; the provider owns the created
    /// services.
    /// Thread Safety: Not thread-safe; intended for single-threaded startup.
    class TBX_API AppCoreServiceFactory
    {
      public:
        explicit AppCoreServiceFactory(AppServiceProvider& services);

        /// @brief
        /// Purpose: Creates (or reuses any host-injected) core services in dependency order and
        /// registers them on the service provider, returning the handles the application caches.
        /// @details
        /// root_directory seeds the file operator; startup_asset_directories adds extra asset
        /// roots (e.g. a settings file's own directory).
        AppCoreServices create(
            const std::filesystem::path& root_directory,
            std::vector<std::filesystem::path> startup_asset_directories);

      private:
        AppServiceProvider& _services;
    };
}
