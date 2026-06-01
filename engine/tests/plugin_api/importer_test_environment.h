#pragma once
#include "in_memory_file_ops.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/registry.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/plugin_api/service_provider.h"

namespace tbx::tests::plugin_api
{
    using InMemoryFileOps = ::tbx::tests::InMemoryFileOps;

    /// @brief
    /// Purpose: Returns a deterministic absolute-style working directory path for tests on each
    /// platform. Ownership: Returns a value path object with no shared lifetime requirements.
    /// Thread Safety: Thread-safe; no shared mutable state.
    static std::filesystem::path get_test_working_directory()
    {
#if defined(_WIN32)
        return std::filesystem::path("C:/virtual/assets");
#else
        return std::filesystem::path("/virtual/assets");
#endif
    }

    /// @brief
    /// Purpose: Populates a service provider with core runtime services for importer tests.
    /// Ownership: The provided service provider owns all registered service instances.
    /// Thread Safety: Not thread-safe; intended for single-threaded test setup and execution.
    static void populate_test_service_provider(
        ServiceProvider& service_provider,
        const std::filesystem::path& working_directory)
    {
        service_provider.register_service<IMessageCoordinator>(
            std::make_shared<MessageCoordinator>());
        service_provider.register_service<IFileOps>(
            std::make_shared<InMemoryFileOps>(working_directory));
        service_provider.register_service<EntityRegistry>(std::make_shared<EntityRegistry>());
        service_provider.register_service<SerializationRegistry>(
            std::make_shared<SerializationRegistry>());
        auto message_coordinator = service_provider.get_service<IMessageCoordinator>().lock();
        auto serialization_registry = service_provider.get_service<SerializationRegistry>().lock();
        if (!message_coordinator || !serialization_registry)
            return;

        service_provider.register_service<AssetManager>(std::make_shared<AssetManager>(
            service_provider.get_service<IMessageCoordinator>(),
            service_provider.get_service<SerializationRegistry>(),
            working_directory));
        service_provider.register_service<JobSystem>(std::make_shared<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_shared<ThreadManager>());
    }
}
