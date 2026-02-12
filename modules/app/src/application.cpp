#include "tbx/app/application.h"
#include "tbx/app/app_events.h"
#include "tbx/app/app_requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/file_ops.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/time/delta_time.h"
#include <algorithm>
#include <chrono>
#include <utility>

namespace tbx
{
    AppSettings::AppSettings(
        IMessageDispatcher& dispatcher,
        bool vsync,
        GraphicsApi api,
        Size resolution)
        : vsync_enabled(&dispatcher, this, &AppSettings::vsync_enabled, vsync)
        , graphics_api(&dispatcher, this, &AppSettings::graphics_api, api)
        , resolution(&dispatcher, this, &AppSettings::resolution, resolution)
    {
    }

    Application::Application(const AppDescription& desc)
        : _name(desc.name)
        , _main_window(
              _msg_coordinator,
              desc.name.empty() ? std::string("Toybox Application") : desc.name,
              {1280, 720},
              WindowMode::WINDOWED,
              false)
        , _settings(_msg_coordinator)
        , _asset_manager(desc.working_root)
    {
        FileOperator file_operator = FileOperator(desc.working_root);
        _settings.working_directory = file_operator.get_working_directory();
        if (desc.logs_directory.empty())
            _settings.logs_directory = file_operator.resolve("logs");
        else
            _settings.logs_directory = file_operator.resolve(desc.logs_directory);

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
            GlobalDispatcherScope scope(_msg_coordinator);

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

    AppSettings& Application::get_settings()
    {
        return _settings;
    }

    IMessageCoordinator& Application::get_message_coordinator()
    {
        return _msg_coordinator;
    }

    EntityRegistry& Application::get_entity_registry()
    {
        return _entity_registry;
    }

    AssetManager& Application::get_asset_manager()
    {
        return _asset_manager;
    }

    void Application::initialize(const std::vector<std::string>& requested_plugins)
    {
        const auto startup_begin = std::chrono::steady_clock::now();
        try
        {
            GlobalDispatcherScope scope(_msg_coordinator);

            TBX_TRACE_INFO("Initializing application: {}", _name);
#if defined(TBX_RELEASE)
            TBX_TRACE_INFO("Build Configuration: Release");
#elif defined(TBX_DEBUG)
            TBX_TRACE_INFO("Build Configuration: Debug");
#endif

            // Register app message handler
            _msg_coordinator.register_handler(
                [this](Message& msg)
                {
                    recieve_message(msg);
                });

            // Load requested plugins
            _loaded = load_plugins(
                _settings.working_directory,
                requested_plugins,
                _settings.working_directory,
                *this);
            for (const auto& loaded : _loaded)
                _asset_manager.add_asset_directory(loaded.meta.resource_directory);

            // Log filesystem directories
            TBX_TRACE_INFO("Working Directory: {}", _settings.working_directory.string());
            TBX_TRACE_INFO("Logs Directory: {}", _settings.logs_directory.string());
            auto asset_roots = _asset_manager.get_asset_directories();
            for (const auto& root : asset_roots)
                TBX_TRACE_INFO("Asset Directory: {}", root.string());

            // Tell everyone we're initialized
            _msg_coordinator.send<ApplicationInitializedEvent>(this);

            const auto startup_elapsed_ms = std::chrono::duration<double, std::milli>(
                                                std::chrono::steady_clock::now() - startup_begin)
                                                .count();
            TBX_TRACE_INFO("Application startup completed in {:.2f} ms.", startup_elapsed_ms);
        }
        catch (const std::exception& ex)
        {
            const auto startup_elapsed_ms = std::chrono::duration<double, std::milli>(
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
        // Process messages posted in previous frame
        _msg_coordinator.flush();

        // Update delta time
        DeltaTime dt = timer.tick();
        _time_running += dt.seconds;
        _asset_unload_elapsed_seconds += dt.seconds;

        // Begin update
        _msg_coordinator.send<ApplicationUpdateBeginEvent>(this, dt);

        // Update all loaded plugins
        for (auto& p : _loaded)
            p.instance->update(dt);

        // End update
        _msg_coordinator.send<ApplicationUpdateEndEvent>(this, dt);

        ++_update_count;
        if (_asset_unload_elapsed_seconds >= 1.0)
        {
            _asset_manager.unload_unreferenced();
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
        constexpr double PERFORMANCE_LOG_INTERVAL_SECONDS = 10.0;
#else
        constexpr double PERFORMANCE_LOG_INTERVAL_SECONDS = 120.0;
#endif
        if (_performance_sample_elapsed_seconds >= PERFORMANCE_LOG_INTERVAL_SECONDS)
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

    void Application::shutdown()
    {
        const auto shutdown_begin = std::chrono::steady_clock::now();
        try
        {
            // IMPORTANT: Shutdown order matters, careful re-arranging could break things.

            GlobalDispatcherScope scope(_msg_coordinator);

            TBX_TRACE_INFO("Shutting down application: {}", _name);
            TBX_TRACE_INFO(
                "Total Run Time: {:.2f}s, Total Updates: {}",
                _time_running,
                _update_count);

            // 1. Send shutdown event
            _msg_coordinator.send<ApplicationShutdownEvent>(this);

            // 2. Close main window
            _main_window.is_open = false;
            _should_exit = true;

            // 3. Destroy all entities
            _entity_registry.destroy_all();
            _asset_manager.unload_all();

            // 4. Detach and unload all non-logging plugins first, then logging plugins.
            _loaded.clear();

            // 5. Process any remaining messages that may have been posted during shutdown
            const auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - shutdown_begin)
                                                 .count();
            TBX_TRACE_INFO("Application shutdown completed in {:.2f} ms.", shutdown_elapsed_ms);

            // 6. Process any remaining posted messages and clear handlers
            _msg_coordinator.flush();
            _msg_coordinator.clear_handlers();
        }
        catch (const std::exception& ex)
        {
            const auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
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
            const auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                                 std::chrono::steady_clock::now() - shutdown_begin)
                                                 .count();
            TBX_TRACE_ERROR(
                "Application shutdown failed after {:.2f} ms with unknown exception.",
                shutdown_elapsed_ms);
            TBX_ASSERT(false, "Unknown exception during application shutdown.");
        }
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
            return;
        }
    }

}
