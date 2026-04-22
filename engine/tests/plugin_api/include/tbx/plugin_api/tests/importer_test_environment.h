#pragma once
#include "tbx/interfaces/input_manager.h"
#include "tbx/systems/app/message_coordinator.h"
#include "tbx/systems/app/settings.h"
#include "tbx/systems/assets/builtin_assets.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/ecs/entity_registry.h"
#include "tbx/systems/files/tests/in_memory_file_ops.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>


namespace tbx::tests::plugin_api
{
    using InMemoryFileOps = ::tbx::tests::file_system::InMemoryFileOps;

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
    /// Purpose: Creates a service provider exposing core runtime services for importer tests.
    /// Ownership: Returned provider owns all registered service instances.
    /// Thread Safety: Not thread-safe; intended for single-threaded test setup and execution.
    static ServiceProvider make_test_service_provider(
        const std::filesystem::path& working_directory)
    {
        auto service_provider = ServiceProvider {};

        service_provider.register_service<Handle>(std::make_unique<Handle>(BoxIcon::HANDLE));
        service_provider.register_service<IMessageCoordinator>(
            std::make_unique<AppMessageCoordinator>());
        service_provider.register_service<EntityRegistry>(std::make_unique<EntityRegistry>());
        service_provider.register_service<AssetManager>(std::make_unique<AssetManager>(
            &service_provider.get_service<IMessageCoordinator>(),
            working_directory));
        service_provider.register_service<AppSettings>(std::make_unique<AppSettings>(
            service_provider.get_service<IMessageCoordinator>(),
            true,
            GraphicsApi::OPEN_GL,
            Size {640, 480}));
        auto& settings = service_provider.get_service<AppSettings>();
        settings.paths.working_directory = working_directory;
        settings.paths.logs_directory = working_directory / "logs";
        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }
}
