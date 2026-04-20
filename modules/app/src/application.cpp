#include "tbx/app/application.h"
#include "tbx/app/events.h"
#include "tbx/app/requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/ops.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/time/delta_time.h"
#include <algorithm>
#include <chrono>
#include <memory>

namespace tbx
{
    static std::filesystem::path get_default_asset_directory()
    {
#if defined(TBX_RELEASE)
        return std::filesystem::path("resources");
#elif defined(TBX_RESOURCES)
        return std::filesystem::path(TBX_RESOURCES).lexically_normal();
#else
        return {};
#endif
    }

    static ServiceProvider create_service_provider(const AppDescription& desc)
    {
        auto service_provider = ServiceProvider {};

        service_provider.register_service<Handle>(std::make_unique<Handle>(desc.icon));
        service_provider.register_service<IMessageCoordinator>(
            std::make_unique<AppMessageCoordinator>());
        service_provider.register_service<EntityRegistry>(std::make_unique<EntityRegistry>());
        service_provider.register_service<InputManager>(
            std::make_unique<InputManager>(service_provider.get_service<IMessageCoordinator>()));
        service_provider.register_service<AssetManager>(
            std::make_unique<AssetManager>(
                &service_provider.get_service<IMessageCoordinator>(),
                desc.working_root));
        service_provider.register_service<AppSettings>(
            std::make_unique<AppSettings>(
                service_provider.get_service<IMessageCoordinator>(),
                false,
                GraphicsApi::OPEN_GL,
                Size {0, 0}));
        service_provider.register_service<JobSystem>(std::make_unique<JobSystem>());
        service_provider.register_service<ThreadManager>(std::make_unique<ThreadManager>());

        return service_provider;
    }

    Application::Application(const AppDescription& desc)
        : _name(desc.name)
        , _service_provider(create_service_provider(desc))
        , _plugin_manager(_service_provider)
        , _main_window(
              _service_provider.get_service<IMessageCoordinator>(),
              desc.name.empty() ? std::string("Toybox Application") : desc.name,
              {1280, 720},
              WindowMode::WINDOWED,
              false)
    {
        auto& settings = _service_provider.get_service<AppSettings>();
        const auto file_operator = FileOperator(desc.working_root);
        settings.paths.working_directory = file_operator.get_working_directory();
        if (desc.logs_directory.empty())
            settings.paths.logs_directory = file_operator.resolve("logs");
        else
            settings.paths.logs_directory = file_operator.resolve(desc.logs_directory);

        add_default_asset_directory();

        for (auto& arg : desc.args)
        {
            // TODO:
            // -- headless
            // -- screenshot count seconds-between
            // -- close-after time-in-milliseconds
            // -- benchmark
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
            auto& msg_coordinator = _service_provider.get_service<IMessageCoordinator>();
            GlobalDispatcherScope scope(msg_coordinator);

            auto timer = DeltaTimer();
            _main_window.is_open = true;

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

    ServiceProvider& Application::get_service_provider()
    {
        return _service_provider;
    }

    const ServiceProvider& Application::get_service_provider() const
    {
        return _service_provider;
    }

    void Application::add_default_asset_directory()
    {
        const auto resource_directory = get_default_asset_directory();
        if (resource_directory.empty())
            return;

        _service_provider.get_service<AssetManager>().add_directory(resource_directory);
    }

    void Application::initialize(const std::vector<std::string>& requested_plugins)
    {
        const auto startup_begin = std::chrono::steady_clock::now();
        auto& msg_coordinator = _service_provider.get_service<IMessageCoordinator>();
        auto& settings = _service_provider.get_service<AppSettings>();
        auto& asset_manager = _service_provider.get_service<AssetManager>();

        try
        {
            GlobalDispatcherScope scope(msg_coordinator);

            TBX_TRACE_INFO("Initializing application: {}", _name);
#if defined(TBX_RELEASE)
            TBX_TRACE_INFO("Build Configuration: Release");
#elif defined(TBX_DEBUG)
            TBX_TRACE_INFO("Build Configuration: Debug");
#endif

            // Register app message handler
            msg_coordinator.register_handler(
                [this](Message& msg)
                {
                    recieve_message(msg);
                });

            // Load requested plugins
            _plugin_manager.load(
                settings.paths.working_directory,
                requested_plugins,
                settings.paths.working_directory);

            // Log filesystem directories
            TBX_TRACE_INFO("Working Directory: '{}'", settings.paths.working_directory.string());
            TBX_TRACE_INFO("Logs Directory: '{}'", settings.paths.logs_directory.string());
            auto asset_roots = asset_manager.get_directories();
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
            msg_coordinator.send<ApplicationInitializedEvent>(this);

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
            TBX_ASSERT(false, "Unknown exception during application initialization.");
        }
    }

    void Application::update(DeltaTimer& timer)
    {
        auto& msg_coordinator = _service_provider.get_service<IMessageCoordinator>();
        auto& input_manager = _service_provider.get_service<InputManager>();
        auto& asset_manager = _service_provider.get_service<AssetManager>();

        // Process messages posted in previous frame
        msg_coordinator.flush();

        // Update delta time
        DeltaTime dt = timer.tick();
        _time_running += dt.seconds;
        _asset_unload_elapsed_seconds += dt.seconds;

        // Begin update
        msg_coordinator.send<ApplicationUpdateBeginEvent>(this, dt);

        // Run fixed update logic
        fixed_update(dt);

        // Update all loaded plugins
        _plugin_manager.update(dt);

        input_manager.update(dt);

        // End update
        msg_coordinator.send<ApplicationUpdateEndEvent>(this, dt);

        // Gather metrics
        ++_update_count;
        if (_asset_unload_elapsed_seconds >= 1.0)
        {
            asset_manager.unload_unreferenced();
            _asset_unload_elapsed_seconds = 0.0;
        }

        ++_performance_sample_frame_count;
        _performance_sample_elapsed_seconds += dt.seconds;

        if (!_performance_sample_has_data)
        {
            _performance_sample_min_frame_time_ms = dt.milliseconds;
            _performance_sample_max_frame_time_ms = dt.milliseconds;
            _performance_sample_has_data = true;
        }
        else
        {
            _performance_sample_min_frame_time_ms =
                std::min(_performance_sample_min_frame_time_ms, dt.milliseconds);
            _performance_sample_max_frame_time_ms =
                std::max(_performance_sample_max_frame_time_ms, dt.milliseconds);
        }

        // Log performance metrics
#ifdef TBX_DEBUG
        constexpr double performance_log_interval_seconds = 10.0;
#else
        constexpr double performance_log_interval_seconds = 60.0;
#endif
        if (_performance_sample_elapsed_seconds >= performance_log_interval_seconds)
        {
            double average_fps = 0.0;
            double average_frame_time_ms = 0.0;

            if (_performance_sample_elapsed_seconds > 0.0 && _performance_sample_frame_count > 0U)
            {
                average_fps = static_cast<double>(_performance_sample_frame_count)
                              / _performance_sample_elapsed_seconds;
                average_frame_time_ms = (_performance_sample_elapsed_seconds * 1000.0)
                                        / static_cast<double>(_performance_sample_frame_count);
            }

            TBX_TRACE_INFO(
                "FPS(avg): {:.2f}, Frame Time(avg): {:.2f}ms, Frame Time(min/max): {:.2f}/{:.2f}ms",
                average_fps,
                average_frame_time_ms,
                _performance_sample_min_frame_time_ms,
                _performance_sample_max_frame_time_ms);

            _performance_sample_elapsed_seconds = 0.0;
            _performance_sample_frame_count = 0U;
            _performance_sample_min_frame_time_ms = 0.0;
            _performance_sample_max_frame_time_ms = 0.0;
            _performance_sample_has_data = false;

            // Warn if average FPS is below 30
            if (average_fps < 30.0)
            {
                TBX_TRACE_WARNING(
                    "Average FPS is below 30! Consider optimizing your application or "
                    "investigating potential performance issues.");
            }
        }
    }

    void Application::fixed_update(const DeltaTime& dt)
    {
        auto& physics_settings = _service_provider.get_service<AppSettings>().physics;
        double fixed_step_seconds =
            std::max(0.0001, static_cast<double>(physics_settings.fixed_time_step_seconds.value));
        int max_sub_steps = std::max(1, static_cast<int>(physics_settings.max_sub_steps.value));

        _fixed_update_accumulator_seconds += dt.seconds;
        if (_fixed_update_accumulator_seconds < fixed_step_seconds)
            return;

        int sub_step_count = 0;
        while (_fixed_update_accumulator_seconds >= fixed_step_seconds
               && sub_step_count < max_sub_steps)
        {
            DeltaTime fixed_dt = {
                .seconds = fixed_step_seconds,
                .milliseconds = fixed_step_seconds * 1000.0,
            };

            _plugin_manager.fixed_update(fixed_dt);

            _fixed_update_accumulator_seconds -= fixed_step_seconds;
            ++sub_step_count;
        }

        double max_accumulator_seconds =
            static_cast<double>(fixed_step_seconds) * static_cast<double>(max_sub_steps);
        if (_fixed_update_accumulator_seconds > max_accumulator_seconds)
            _fixed_update_accumulator_seconds = max_accumulator_seconds;
    }

    void Application::shutdown()
    {
        auto& msg_coordinator = _service_provider.get_service<IMessageCoordinator>();
        auto& entity_registry = _service_provider.get_service<EntityRegistry>();
        auto& asset_manager = _service_provider.get_service<AssetManager>();
        auto& thread_manager = _service_provider.get_service<ThreadManager>();
        GlobalDispatcherScope scope(msg_coordinator);
        const auto shutdown_begin = std::chrono::steady_clock::now();

        try
        {
            // IMPORTANT: Shutdown order matters, careful re-arranging could break things.

            TBX_TRACE_INFO("Shutting down application: {}", _name);
            TBX_TRACE_INFO(
                "Total Run Time: {:.2f}s, Total Updates: {}",
                _time_running,
                _update_count);

            // 1. Send shutdown event
            msg_coordinator.send<ApplicationShutdownEvent>(this);

            // 2. Close main window
            //_main_window.is_open = false;
            _should_exit = true;

            // 3. Detach and unload plugins using dependency-aware unload ordering.
            _plugin_manager.unload_all();

            // 4. Unregister all entities and unload assets after plugin teardown.
            entity_registry.clear();
            asset_manager.unload_all();

            // 5. Stop dedicated thread lanes after plugin teardown.
            thread_manager.stop_all();

            // 6. Process any remaining posted messages and clear handlers
            msg_coordinator.flush();
            msg_coordinator.clear_handlers();
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
            TBX_ASSERT(false, "Unknown exception during application shutdown.");
        }

        // 7. Log shutdown metrics
        auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - shutdown_begin)
                                       .count();
        TBX_TRACE_INFO("Application shutdown completed in {:.2f} ms.", shutdown_elapsed_ms);
        TBX_TRACE_FLUSH();
    }

    void Application::recieve_message(Message& msg)
    {
        if (auto* exit_request = handle_message<ExitApplicationRequest>(msg))
        {
            _should_exit = true;
            exit_request->state = MessageState::HANDLED;
            return;
        }

        if (auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            if (open_event->owner == &_main_window && !open_event->current)
                _should_exit = true;
        }

        _plugin_manager.receive_message(msg);
    }
}
