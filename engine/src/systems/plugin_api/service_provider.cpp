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
#include <algorithm>
#include <filesystem>
#include <vector>

namespace tbx
{
    ServiceProvider::ServiceProvider(DefaultServicesTag)
    {
        register_service<IMessageCoordinator>(std::make_shared<MessageCoordinator>());

        register_service<IFileOps>(std::make_shared<FileOperator>(std::filesystem::path()));
        auto file_ops = get_service<IFileOps>().lock();

        register_service<SerializationRegistry>(std::make_shared<SerializationRegistry>(file_ops));

        register_service<PluginOwnershipTracker>(std::make_shared<PluginOwnershipTracker>());
        bind_plugin_ownership_tracker(get_service<PluginOwnershipTracker>());

        register_service<AssetManager>(std::make_shared<AssetManager>(
            get_service<IMessageCoordinator>(),
            get_service<SerializationRegistry>(),
            std::filesystem::path(),
            std::vector<std::filesystem::path>(),
            HandleSource(),
            file_ops));

        register_service<WorldManager>(
            std::make_shared<WorldManager>(try_get_service<AssetManager>()));

        register_service<ScriptSystem>(std::make_shared<ScriptSystem>(
            try_get_service<AssetManager>(),
            *this,
            try_get_service<WorldManager>()));

        register_service<JobSystem>(std::make_shared<JobSystem>());
        register_service<ThreadManager>(std::make_shared<ThreadManager>());
    }

    ServiceProvider::~ServiceProvider() noexcept
    {
        clear();
    }

    ServiceProvider create_default_service_provider()
    {
        return ServiceProvider(ServiceProvider::DefaultServicesTag {});
    }

    void ServiceProvider::deregister_service(std::type_index service_type)
    {
        erase_service(service_type);
    }

    void ServiceProvider::clear()
    {
        while (!_registration_order.empty())
        {
            const auto service_type = _registration_order.back();
            erase_service(service_type);
        }

        TBX_ASSERT(
            _entries.empty(),
            "Service provider still has {} services without registration order.",
            _entries.size());
        _entries.clear();
    }

    void ServiceProvider::erase_service(std::type_index service_type)
    {
        const auto it = _entries.find(service_type);
        if (it == _entries.end())
        {
            forget_registration_order(service_type);
            return;
        }

        const long use_count = it->second->service_use_count();
        TBX_ASSERT(
            use_count == 1,
            "Service type '{}' still has {} strong references during cleanup.",
            it->second->service_type().name(),
            use_count);

        _entries.erase(it);
        forget_registration_order(service_type);
    }

    void ServiceProvider::forget_registration_order(std::type_index service_type)
    {
        std::erase(_registration_order, service_type);
    }

    void ServiceProvider::remember_registration_order(std::type_index service_type)
    {
        forget_registration_order(service_type);
        _registration_order.push_back(service_type);
    }
}
