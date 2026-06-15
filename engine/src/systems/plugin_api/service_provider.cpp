#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/world/manager.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/scripting/script_system.h"
#include <algorithm>
#include <filesystem>
#include <vector>

namespace tbx
{
    ServiceProvider::~ServiceProvider() noexcept
    {
        clear();
    }

    void register_default_services(ServiceProvider& service_provider)
    {
        auto file_ops = service_provider.try_get_service<IFileOps>().lock();
        if (!file_ops)
        {
            file_ops = std::make_shared<FileOperator>(std::filesystem::path());
            service_provider.register_service<IFileOps>(file_ops);
        }

        auto message_coordinator = service_provider.try_get_service<IMessageCoordinator>().lock();
        if (!message_coordinator)
        {
            message_coordinator = std::make_shared<MessageCoordinator>();
            service_provider.register_service<IMessageCoordinator>(message_coordinator);
        }

        auto serialization_registry =
            service_provider.try_get_service<SerializationRegistry>().lock();
        if (!serialization_registry)
        {
            serialization_registry = std::make_shared<SerializationRegistry>(file_ops);
            service_provider.register_service<SerializationRegistry>(serialization_registry);
        }

        auto asset_manager = service_provider.try_get_service<AssetManager>().lock();
        if (!asset_manager)
        {
            asset_manager = std::make_shared<AssetManager>(
                message_coordinator,
                serialization_registry,
                file_ops->get_working_directory(),
                std::vector<std::filesystem::path> {},
                HandleSource(),
                file_ops);
            service_provider.register_service<AssetManager>(asset_manager);
        }

        auto world_manager = service_provider.try_get_service<WorldManager>().lock();
        if (!world_manager)
        {
            world_manager = std::make_shared<WorldManager>(asset_manager, message_coordinator);
            service_provider.register_service<WorldManager>(world_manager);
        }

        if (service_provider.try_get_service<JobSystem>().expired())
            service_provider.register_service<JobSystem>(std::make_shared<JobSystem>());
        if (service_provider.try_get_service<ThreadManager>().expired())
            service_provider.register_service<ThreadManager>(std::make_shared<ThreadManager>());
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
