#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/world/manager.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/plugin_api/plugin_ownership_tracker.h"
#include "tbx/systems/scripting/script_system.h"
#include <filesystem>
#include <vector>

namespace tbx
{
    ServiceProvider create_default_service_provider()
    {
        auto service_provider = ServiceProvider {};

        service_provider.register_service<IMessageCoordinator>(
            std::make_unique<MessageCoordinator>());

        service_provider.register_service<IFileOps>(
            std::make_unique<FileOperator>(std::filesystem::path()));
        auto file_ops = service_provider.get_service<IFileOps>().lock();

        service_provider.register_service<SerializationRegistry>(
            std::make_unique<SerializationRegistry>(file_ops));

        service_provider.register_service<PluginOwnershipTracker>(
            std::make_unique<PluginOwnershipTracker>());
        bind_plugin_ownership_tracker(service_provider.get_service<PluginOwnershipTracker>());

        service_provider.register_service<AssetManager>(std::make_unique<AssetManager>(
            service_provider.get_service<IMessageCoordinator>(),
            service_provider.get_service<SerializationRegistry>(),
            std::filesystem::path(),
            std::vector<std::filesystem::path>(),
            HandleSource(),
            file_ops));

        service_provider.register_service<WorldManager>(
            std::make_unique<WorldManager>(service_provider.try_get_service<AssetManager>()));

        service_provider.register_service<ScriptSystem>(std::make_unique<ScriptSystem>(
            service_provider.try_get_service<AssetManager>(),
            service_provider,
            service_provider.try_get_service<WorldManager>()));

        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }

    void ServiceProvider::deregister_service(std::type_index service_type)
    {
        _entries.erase(service_type);
    }

    void ServiceProvider::clear()
    {
        _entries.clear();
    }
}
