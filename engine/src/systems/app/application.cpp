#include "tbx/systems/app/application.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/time/delta_time.h"
#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>

namespace tbx
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
        service_provider.register_service<EntityRegistry>(std::make_unique<EntityRegistry>());
        service_provider.register_service<SerializationRegistry>(
            std::make_unique<SerializationRegistry>());
        auto message_coordinator = service_provider.get_service<IMessageCoordinator>().lock();
        auto serialization_registry =
            service_provider.get_service<SerializationRegistry>().lock();
        TBX_ASSERT(
            message_coordinator != nullptr && serialization_registry != nullptr,
            "Core services must be available before registering dependent services.");
        if (!message_coordinator || !serialization_registry)
            return service_provider;

        service_provider.register_service<AssetManager>(std::make_unique<AssetManager>(
            *message_coordinator,
            *serialization_registry,
            desc.working_root));
        auto settings = std::make_unique<AppSettings>(
            *message_coordinator,
            false,
            GraphicsApi::OPEN_GL,
            Size {0, 0});
        settings->icon = desc.icon;
        service_provider.register_service<AppSettings>(std::move(settings));
        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }

    Application::Application(const AppDescription& desc)
        : _name(desc.name)
        , _service_provider(create_service_provider(desc))
        , _plugin_manager(_service_provider)
    {
        _msg_coordinator = _service_provider.get_service<IMessageCoordinator>();
        _settings = _service_provider.get_service<AppSettings>();
        _asset_manager = _service_provider.get_service<AssetManager>();
        _entity_registry = _service_provider.get_service<EntityRegistry>();
        _thread_manager = _service_provider.get_service<ThreadManager>();

        auto settings = _settings.lock();
        auto asset_manager = _asset_manager.lock();
        TBX_ASSERT(
            settings != nullptr && asset_manager != nullptr,
            "Application core services must be registered before initialization.");
        if (!settings || !asset_manager)
            return;

        const auto file_operator = FileOperator(desc.working_root);
        settings->paths.working_directory = file_operator.get_working_directory();
        if (desc.logs_directory.empty())
            settings->paths.logs_directory = file_operator.resolve("logs");
        else
            settings->paths.logs_directory = file_operator.resolve(desc.logs_directory);

        const auto resource_directory = get_default_asset_directory();
        if (!resource_directory.empty())
            asset_manager->add_directory(resource_directory);

        if (!desc.args.empty())
        {
            TBX_TRACE_INFO("Arguments:");
            for (const auto& arg : desc.args)
            {
                // TODO:
                // -- headless
                // -- screenshot count seconds-between
                // -- close-after time-in-milliseconds
                // -- benchmark
                TBX_TRACE_INFO("    -{}", arg);
            }
        }

        initialize(desc.requested_plugins);
    }

    Application::~Application() noexcept
    {
        shutdown();
    }

    int Application::run()
    {
        try
        {
            auto timer = DeltaTimer();
            if (!_main_window.is_valid())
            {
                TBX_TRACE_ERROR("Application main window must be valid before run.");
                _should_exit = true;
                TBX_ASSERT(
                    _main_window.is_valid(),
                    "Application main window must be valid before run.");
                return -1;
            }

            auto window_manager = _window_manager.lock();
            if (!window_manager)
            {
                TBX_TRACE_ERROR("Application requires an IWindowManager service.");
                _should_exit = true;
                TBX_ASSERT(window_manager != nullptr, "Application requires an IWindowManager service.");
                return -1;
            }

            if (!window_manager->is_open(_main_window))
            {
                TBX_TRACE_ERROR("Application main window must be open before run.");
                _should_exit = true;
                return -1;
            }

            while (!_should_exit)
            {
                update(timer);
            }

            return 0;
        }
        catch (const std::exception& ex)
        {
            TBX_TRACE_ERROR("Unhandled exception in application run loop: {}", ex.what());
            return -1;
        }
        catch (...)
        {
            TBX_TRACE_ERROR("Unknown unhandled exception in application run loop.");
            return -1;
        }
    }

    const std::string& Application::get_name() const
    {
        return _name;
    }

    const Window& Application::get_main_window() const
    {
        return _main_window;
    }

    ServiceProvider& Application::get_service_provider()
    {
        return _service_provider;
    }

    const ServiceProvider& Application::get_service_provider() const
    {
        return _service_provider;
    }

    void Application::initialize(const std::vector<std::string>& requested_plugins)
    {
        const auto startup_begin = std::chrono::steady_clock::now();
        auto msg_coordinator = _msg_coordinator.lock();
        auto settings = _settings.lock();
        auto asset_manager = _asset_manager.lock();
        auto entity_registry = _entity_registry.lock();
        if (!msg_coordinator || !settings || !asset_manager || !entity_registry)
        {
            TBX_TRACE_ERROR("Application core services are unavailable during initialization.");
            _should_exit = true;
            TBX_ASSERT(
                msg_coordinator != nullptr && settings != nullptr && asset_manager != nullptr
                    && entity_registry != nullptr,
                "Application core services are unavailable during initialization.");
            return;
        }

        try
        {
            TBX_TRACE_INFO("Initializing application: {}", _name);
#if defined(TBX_RELEASE)
            TBX_TRACE_INFO("Build Configuration: Release");
#elif defined(TBX_DEBUG)
            TBX_TRACE_INFO("Build Configuration: Debug");
#endif

            // Register app message handler
            msg_coordinator->register_handler(
                [this](Message& msg)
                {
                    if (auto exit_request = handle_message<ExitApplicationRequest>(msg))
                    {
                        _should_exit = true;
                        exit_request->get().state = MessageState::HANDLED;
                        return;
                    }

                    if (auto closed_event = handle_message<WindowClosedEvent>(msg))
                    {
                        if (closed_event->get().window == _main_window)
                            _should_exit = true;
                    }

                    _plugin_manager.receive_message(msg);
                });

            // Load requested plugins
            _plugin_manager.load(
                settings->paths.working_directory,
                requested_plugins,
                settings->paths.working_directory);
            _input_manager = _service_provider.try_get_service<IInputManager>();

            // Setup physics
            {
                if (!_service_provider.has_service<Physics>())
                {
                    if (auto physics_backend =
                            _service_provider.try_get_service<IPhysicsBackend>().lock())
                    {
                        _service_provider.register_service<Physics>(std::make_unique<Physics>(
                            *physics_backend,
                            *entity_registry,
                            *asset_manager,
                            *settings));
                    }
                }

                _physics = _service_provider.try_get_service<Physics>();
            }

            // Open main window
            {
                auto window_manager = _service_provider.try_get_service<IWindowManager>();
                auto window_manager_strong = window_manager.lock();
                if (!window_manager_strong)
                {
                    TBX_TRACE_ERROR("Application requires an IWindowManager service.");
                    _should_exit = true;
                    TBX_ASSERT(
                        window_manager_strong != nullptr,
                        "Application requires an IWindowManager service.");
                    return;
                }

                _window_manager = window_manager;

                const auto main_window_title =
                    _name.empty() ? std::string("Toybox Application") : _name;
                _main_window = window_manager_strong->open(
                    WindowCreateInfo {
                        .title = main_window_title,
                        .size = {1280, 720},
                        .mode = WindowMode::WINDOWED,
                    });
            }

            // Setup rendering
            if (auto graphics_backend = _service_provider.try_get_service<IGraphicsBackend>().lock())
            {
                auto window_manager = _window_manager.lock();
                auto thread_manager = _thread_manager.lock();
                if (!window_manager || !thread_manager)
                {
                    TBX_TRACE_ERROR(
                        "Application requires window and thread services for rendering.");
                    _should_exit = true;
                    TBX_ASSERT(
                        window_manager != nullptr && thread_manager != nullptr,
                        "Application requires window and thread services for rendering.");
                    return;
                }

                _service_provider.register_service<Rendering>(std::make_unique<Rendering>(
                    *graphics_backend,
                    *entity_registry,
                    *asset_manager,
                    *thread_manager,
                    *window_manager,
                    _main_window,
                    settings->graphics));
                _rendering = _service_provider.try_get_service<Rendering>();
            }

            // Log filesystem directories
            TBX_TRACE_INFO("Working Directory: '{}'", settings->paths.working_directory.string());
            TBX_TRACE_INFO("Logs Directory: '{}'", settings->paths.logs_directory.string());
            auto asset_roots = asset_manager->get_directories();
            if (asset_roots.size() > 1)
            {
                TBX_TRACE_INFO("Asset Directories:");
                for (const auto& root : asset_roots)
                    TBX_TRACE_INFO("    -'{}'", root.string());
            }
            else if (!asset_roots.empty())
                TBX_TRACE_INFO("Asset Directory: {}", asset_roots.front().string());
            else
                TBX_TRACE_INFO("Asset Directory: <none>");

            // Tell everyone we're initialized
            msg_coordinator->send<ApplicationInitializedEvent>(*this);

            auto startup_elapsed_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - startup_begin)
                                          .count();
            TBX_TRACE_INFO("Application startup completed in {:.2f} ms.", startup_elapsed_ms);
        }
        catch (const std::exception& ex)
        {
            auto startup_elapsed_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - startup_begin)
                                          .count();
            TBX_TRACE_ERROR(
                "Application startup failed after {:.2f} ms: {}",
                startup_elapsed_ms,
                ex.what());
            _should_exit = true;
            TBX_ASSERT(false, "Exception during application initialization: {}", ex.what());
        }
        catch (...)
        {
            const auto startup_elapsed_ms = std::chrono::duration<double, std::milli>(
                                                std::chrono::steady_clock::now() - startup_begin)
                                                .count();
            TBX_TRACE_ERROR(
                "Application startup failed after {:.2f} ms with unknown exception.",
                startup_elapsed_ms);
            _should_exit = true;
            TBX_ASSERT(false, "Unknown exception during application initialization.");
        }
    }

    void Application::update(DeltaTimer& timer)
    {
        auto msg_coordinator = _msg_coordinator.lock();
        auto asset_manager = _asset_manager.lock();
        if (!msg_coordinator || !asset_manager)
        {
            TBX_TRACE_ERROR("Application update skipped because required services are unavailable.");
            _should_exit = true;
            return;
        }

        // Process messages posted in the previous frame.
        msg_coordinator->flush();

        // Update delta time
        DeltaTime dt = timer.tick();
        _time_running += dt.seconds;
        _asset_unload_elapsed_seconds += dt.seconds;

        // Begin update
        msg_coordinator->send<ApplicationUpdateBeginEvent>(*this, dt);

        // Physics tick
        fixed_update(dt);

        // Run frame systems.
        {
            _plugin_manager.update(dt);
            if (auto window_manager = _window_manager.lock())
                window_manager->update();
            if (auto rendering = _rendering.lock())
                rendering->render();
            if (auto input_manager = _input_manager.lock())
                input_manager->update(dt);
        }

        // End update
        msg_coordinator->send<ApplicationUpdateEndEvent>(*this, dt);

        // Purge stale assets
        ++_update_count;
        if (_asset_unload_elapsed_seconds >= 1.0)
        {
            asset_manager->unload_unreferenced();
            _asset_unload_elapsed_seconds = 0.0;
        }
    }

    void Application::fixed_update(const DeltaTime& dt)
    {
        auto settings = _settings.lock();
        if (!settings)
            return;

        auto& physics_settings = settings->physics;
        const double fixed_step_seconds =
            std::max(0.0001, static_cast<double>(physics_settings.fixed_time_step_seconds.value));
        const int max_sub_steps =
            std::max(1, static_cast<int>(physics_settings.max_sub_steps.value));

        _fixed_update_accumulator_seconds += dt.seconds;
        int sub_step_count = 0;
        while (_fixed_update_accumulator_seconds >= fixed_step_seconds
               && sub_step_count < max_sub_steps)
        {
            DeltaTime fixed_dt = {
                .seconds = fixed_step_seconds,
                .milliseconds = fixed_step_seconds * 1000.0,
            };

            _plugin_manager.fixed_update(fixed_dt);

            if (auto physics = _physics.lock())
                physics->update(fixed_dt);

            _fixed_update_accumulator_seconds -= fixed_step_seconds;
            ++sub_step_count;
        }

        const double max_accumulator_seconds =
            fixed_step_seconds * static_cast<double>(max_sub_steps);
        if (_fixed_update_accumulator_seconds > max_accumulator_seconds)
            _fixed_update_accumulator_seconds = max_accumulator_seconds;
    }

    void Application::shutdown()
    {
        const auto shutdown_begin = std::chrono::steady_clock::now();
        try
        {
            auto msg_coordinator = _msg_coordinator.lock();
            auto entity_registry = _entity_registry.lock();
            auto asset_manager = _asset_manager.lock();
            auto thread_manager = _thread_manager.lock();
            if (!msg_coordinator || !entity_registry || !asset_manager || !thread_manager)
                return;

            // IMPORTANT: Shutdown order matters, careful re-arranging could break things.
            TBX_TRACE_INFO("Shutting down application: {}", _name);
            TBX_TRACE_INFO(
                "Total Run Time: {:.2f}s, Total Updates: {}",
                _time_running,
                _update_count);

            // 1. Send shutdown event.
            msg_coordinator->send<ApplicationShutdownEvent>(*this);

            // 2. Release renderer-owned graphics resources while the window/context services live.
            _rendering = {};
            if (_service_provider.has_service<Rendering>())
                _service_provider.deregister_service<Rendering>();

            // 3. Close all managed windows.
            if (auto window_manager = _window_manager.lock())
                window_manager->shutdown();
            _main_window = {};
            _window_manager = {};
            _should_exit = true;

            // 4. Release physics resources while the backend plugin is still attached.
            if (_service_provider.has_service<Physics>())
            {
                _physics = {};
                _service_provider.deregister_service<Physics>();
            }

            // 5. Detach plugins while their libraries are still loaded.
            _plugin_manager.detach_all();
            _input_manager = {};

            // 6. Unregister all entities and unload assets after plugin teardown.
            entity_registry->clear();
            asset_manager->unload_all();

            // 7. Unload detached plugin libraries after plugin-authored component state is gone.
            _plugin_manager.unload_all();

            // 8. Stop dedicated thread lanes after plugin teardown.
            thread_manager->stop_all();

            // 9. Process any remaining posted messages and clear handlers.
            msg_coordinator->flush();
            msg_coordinator->clear_handlers();
        }
        catch (const std::exception& ex)
        {
            auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - shutdown_begin)
                                           .count();
            TBX_TRACE_ERROR(
                "Application shutdown failed after {:.2f} ms: {}",
                shutdown_elapsed_ms,
                ex.what());
            _should_exit = true;
            TBX_ASSERT(false, "Exception during application shutdown: {}", ex.what());
        }
        catch (...)
        {
            auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                           std::chrono::steady_clock::now() - shutdown_begin)
                                           .count();
            TBX_TRACE_ERROR(
                "Application shutdown failed after {:.2f} ms with unknown exception.",
                shutdown_elapsed_ms);
            _should_exit = true;
            TBX_ASSERT(false, "Unknown exception during application shutdown.");
        }

        // Log shutdown metrics.
        const auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - shutdown_begin)
                                             .count();
        TBX_TRACE_INFO("Application shutdown completed in {:.2f} ms.", shutdown_elapsed_ms);
        TBX_TRACE_FLUSH();
    }

}
