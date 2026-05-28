#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/application.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/ecs/streamer.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/time/delta_time.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>

namespace tbx::internal
{
    static std::filesystem::path get_default_asset_directory()
    {
#if defined(TBX_RESOURCES)
        return std::filesystem::path(TBX_RESOURCES).lexically_normal();
#else
        return {};
#endif
    }

    static ServiceProvider create_service_provider(const AppDescription& desc)
    {
        auto service_provider = ServiceProvider {};

        service_provider.register_service<IMessageCoordinator>(
            std::make_unique<MessageCoordinator>());
        auto file_ops = std::make_shared<FileOperator>(desc.working_root);
        service_provider.register_service<SerializationRegistry>(
            std::make_unique<SerializationRegistry>(file_ops));
        auto message_coordinator = service_provider.get_service<IMessageCoordinator>().lock();
        auto serialization_registry = service_provider.get_service<SerializationRegistry>().lock();
        TBX_ASSERT(
            message_coordinator != nullptr && serialization_registry != nullptr,
            "Core services must be available before registering dependent services.");
        if (!message_coordinator || !serialization_registry)
            return service_provider;

        service_provider.register_service<AssetManager>(std::make_unique<AssetManager>(
            *message_coordinator,
            *serialization_registry,
            desc.working_root,
            std::vector<std::filesystem::path>(),
            HandleSource(),
            file_ops));
        service_provider.register_service<EntityStreamer>(
            std::make_unique<EntityStreamer>(service_provider.try_get_service<AssetManager>()));
        auto settings = std::make_unique<AppSettings>(
            message_coordinator,
            false,
            GraphicsApi::OPEN_GL,
            Size {0, 0});
#if defined(TBX_DEBUG)
        // Smaller shadow maps keep interactive debug builds closer to real-time on modest GPUs.
        settings->graphics->shadow_map_resolution = 1024U;
#endif
        settings->icon = desc.icon;
        service_provider.register_service<AppSettings>(std::move(settings));
        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }

}
