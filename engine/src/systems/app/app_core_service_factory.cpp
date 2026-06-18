#include "tbx/systems/app/app_core_service_factory.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/types/handle.h"

namespace tbx
{
    AppCoreServiceFactory::AppCoreServiceFactory(AppServiceProvider& services)
        : _services(services)
    {
    }

    AppCoreServices AppCoreServiceFactory::create(
        const std::filesystem::path& root_directory,
        std::vector<std::filesystem::path> startup_asset_directories)
    {
        // Built in dependency order so each factory sees its prerequisites already constructed and
        // registered: file/message -> serialization -> assets -> world/script -> job/thread
        // execution. AppServiceProvider::ensure reuses any instance a host injected.
        auto file_ops = _services.ensure<IFileOps>(
            [&] { return std::make_shared<FileOperator>(root_directory); });

        auto message_coordinator = _services.ensure<IMessageCoordinator>(
            [] { return std::make_shared<MessageCoordinator>(); });

        auto serialization_registry = _services.ensure<SerializationRegistry>(
            [&] { return std::make_shared<SerializationRegistry>(file_ops); });

        auto asset_manager = _services.ensure<AssetManager>(
            [&]
            {
                return std::make_shared<AssetManager>(
                    message_coordinator,
                    serialization_registry,
                    file_ops->get_working_directory(),
                    std::move(startup_asset_directories),
                    HandleSource(),
                    file_ops);
            });

        auto world_manager = _services.ensure<WorldManager>(
            [&] { return std::make_shared<WorldManager>(asset_manager, message_coordinator); });

        auto script_system = _services.ensure<ScriptSystem>(
            [&]
            {
                return std::make_shared<ScriptSystem>(
                    _services.shared(), asset_manager, world_manager, message_coordinator);
            });

        // JobSystem and ThreadManager are registered for later lookup; the application keeps no
        // handle to the job system (Physics is the only consumer and resolves it on demand).
        _services.ensure<JobSystem>([] { return std::make_shared<JobSystem>(); });
        auto thread_manager =
            _services.ensure<ThreadManager>([] { return std::make_shared<ThreadManager>(); });

        return AppCoreServices {
            .file_ops = std::move(file_ops),
            .message_coordinator = std::move(message_coordinator),
            .asset_manager = std::move(asset_manager),
            .world_manager = std::move(world_manager),
            .thread_manager = std::move(thread_manager),
            .script_system = std::move(script_system),
        };
    }
}
