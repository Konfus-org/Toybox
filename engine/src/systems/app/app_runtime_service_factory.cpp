#include "tbx/systems/app/app_runtime_service_factory.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/thread_manager.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/windowing/manager.h"
#include "tbx/systems/world/manager.h"

namespace tbx
{
    AppRuntimeServiceFactory::AppRuntimeServiceFactory(AppServiceProvider& services)
        : _services(services)
    {
    }

    std::optional<AppRuntimeServices> AppRuntimeServiceFactory::create(
        const PhysicsSettings& physics_settings)
    {
        auto& provider = _services.get();
        const auto message_coordinator = provider.try_get_service<IMessageCoordinator>().lock();
        const auto asset_manager = provider.try_get_service<AssetManager>().lock();
        const auto world_manager = provider.try_get_service<WorldManager>().lock();
        const auto thread_manager = provider.try_get_service<ThreadManager>().lock();
        const auto job_system = provider.try_get_service<JobSystem>().lock();

        const auto window_backend = provider.try_get_service<IWindowBackend>();
        const auto physics_backend = provider.try_get_service<IPhysicsBackend>();
        const auto graphics_backend = provider.try_get_service<IGraphicsBackend>();

        auto services = AppRuntimeServices();

        // Windowing must come first because rendering depends on a live window manager, and the
        // backend usually arrives from a plugin that was just loaded.
        if (!window_backend.expired())
        {
            auto window_manager =
                std::make_shared<WindowManager>(message_coordinator, window_backend);
            provider.register_service<IWindowManager>(window_manager);
            services.window_manager = std::move(window_manager);
        }

        // Physics can be created once the plugin backend exists and the core asset/world services
        // are already registered.
        if (!physics_backend.expired())
        {
            auto physics = std::make_shared<Physics>(
                physics_backend,
                asset_manager,
                world_manager,
                thread_manager,
                job_system,
                message_coordinator,
                physics_settings);
            provider.register_service<Physics>(physics);
            services.physics = std::move(physics);
        }

        // Rendering comes last because it needs the graphics backend plus window/thread services
        // that were established earlier in the startup sequence.
        if (!graphics_backend.expired())
        {
            auto window_manager_service = provider.try_get_service<IWindowManager>();
            if (window_manager_service.expired())
            {
                TBX_TRACE_ERROR("Application requires a window service for rendering.");
                return std::nullopt;
            }

            // Gizmos is a shared immediate-mode debug-draw service; tooling (the editor) draws it by
            // registering a render pass on Rendering, so the renderer itself needs no direct handle.
            auto gizmos = std::make_shared<Gizmos>(graphics_backend, asset_manager);
            provider.register_service<Gizmos>(gizmos);

            auto rendering = std::make_shared<Rendering>(
                graphics_backend,
                asset_manager,
                thread_manager,
                window_manager_service,
                world_manager,
                message_coordinator);
            provider.register_service<Rendering>(rendering);
            services.gizmos = std::move(gizmos);
            services.rendering = std::move(rendering);
        }

        // Input is app-owned like windowing/physics/rendering: the InputManager (scheme/action
        // evaluation plus host injection) lives in the engine and reads raw device state from
        // whatever IInputBackend a plugin supplied. Headless runs load no input backend.
        const auto input_backend = provider.try_get_service<IInputBackend>();
        if (!input_backend.expired())
        {
            auto input_manager = std::make_shared<InputManager>(input_backend);
            provider.register_service<InputManager>(input_manager);
            services.input_manager = std::move(input_manager);
        }

        return services;
    }
}
