#include "tbx/systems/app/application.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/systems/app/launch_config.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/reload_queue.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/systems/windowing/manager.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>

namespace tbx
{
    static std::shared_ptr<AppSettings> load_app_settings(
        AssetManager& asset_manager,
        const std::string& settings_asset)
    {
        auto settings = std::make_shared<AppSettings>();
        const auto settings_handle = Handle(settings_asset);
        auto asset_settings = asset_manager.load<AppSettings>(settings_handle);
        if (!asset_settings)
            TBX_TRACE_WARNING("Failed to load application settings '{}'.", settings_asset);
        else
            settings = asset_settings;

        return settings;
    }

    static std::filesystem::path resolve_app_icon_path(
        AssetManager& asset_manager,
        const Handle& icon)
    {
        if (!icon.is_valid())
            return {};

        static_cast<void>(asset_manager.resolve(icon));
        const auto& const_asset_manager = static_cast<const AssetManager&>(asset_manager);
        return const_asset_manager.resolve(icon);
    }

    // TODO: need to make a specialized app service provider that has 2 setup stages startup
    // services and standard services then make early do the default service provider stuff and
    // standard setup the physics and rendering and window manager.
    static void register_app_physics(
        ServiceProvider& service_provider,
        const PhysicsSettings& settings)
    {
        if (service_provider.has_service<Physics>())
            return;

        if (service_provider.try_get_service<IPhysicsBackend>().expired())
            return;

        service_provider.register_service<Physics>(std::make_shared<Physics>(
            service_provider.try_get_service<IPhysicsBackend>(),
            service_provider.try_get_service<AssetManager>(),
            service_provider.try_get_service<WorldManager>(),
            service_provider.try_get_service<AssetReloadQueue>(),
            settings));
    }

    static void register_app_rendering(ServiceProvider& service_provider)
    {
        if (service_provider.has_service<Rendering>())
            return;

        if (service_provider.try_get_service<IGraphicsBackend>().expired())
            return;

        if (service_provider.try_get_service<IWindowManager>().expired()
            || service_provider.try_get_service<ThreadManager>().expired())
        {
            TBX_TRACE_ERROR("Application requires window and thread services for rendering.");
            TBX_ASSERT(false, "Application requires window and thread services for rendering.");
            return;
        }

        service_provider.register_service<Rendering>(std::make_shared<Rendering>(
            service_provider.try_get_service<IGraphicsBackend>(),
            service_provider.try_get_service<AssetManager>(),
            service_provider.try_get_service<ThreadManager>(),
            service_provider.try_get_service<IWindowManager>(),
            service_provider.try_get_service<WorldManager>(),
            service_provider.try_get_service<AssetReloadQueue>()));
    }

    static void register_app_window_manager(ServiceProvider& service_provider)
    {
        if (service_provider.has_service<IWindowManager>())
            return;

        if (service_provider.try_get_service<IWindowBackend>().expired())
            return;

        service_provider.register_service<IWindowManager>(std::make_shared<WindowManager>(
            service_provider.get_service<IMessageCoordinator>(),
            service_provider.try_get_service<IWindowBackend>()));
    }

    Application::Application()
        : _service_provider(create_default_service_provider_shared())
        , _plugin_manager(_service_provider)
    {
        _msg_coordinator = _service_provider->get_service<IMessageCoordinator>();
        _asset_reload_queue = _service_provider->get_service<AssetReloadQueue>();
        _asset_manager = _service_provider->get_service<AssetManager>();
        _world_manager = _service_provider->get_service<WorldManager>();
        _script_system = _service_provider->get_service<ScriptSystem>();
        _thread_manager = _service_provider->get_service<ThreadManager>();

        initialize();
    }

    Application::~Application() noexcept
    {
        shutdown();
    }

    int Application::run()
    {
        try
        {
            auto window_manager = _window_manager.lock();
            if (!window_manager)
            {
                _should_exit = true;
                TBX_ASSERT(false, "Application requires an IWindowManager service.");
                return -1;
            }

            if (!window_manager->has_main_window())
            {
                TBX_ASSERT(false, "Application requires a main window before run.");
                _should_exit = true;
                return -1;
            }

            if (!window_manager->is_open(window_manager->get_main_window()))
            {
                TBX_ASSERT(false, "Application main window must be open before run.");
                _should_exit = true;
                return -1;
            }

            auto timer = DeltaTimer();
            while (!_should_exit)
            {
                update(timer);
            }

            return 0;
        }
        catch (const std::exception& ex)
        {
            TBX_ASSERT(false, "Unhandled exception in application run loop: {}", ex.what());
            return -1;
        }
        catch (...)
        {
            TBX_ASSERT(false, "Unknown unhandled exception in application run loop.");
            return -1;
        }
    }

    const std::string& Application::get_name() const
    {
        return _name;
    }

    const Window& Application::get_main_window() const
    {
        static const auto INVALID_WINDOW = Window();
        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return INVALID_WINDOW;

        return window_manager->get_main_window();
    }

    const AppSettings& Application::get_settings() const
    {
        static const auto DEFAULT_SETTINGS = AppSettings();
        return _settings ? *_settings : DEFAULT_SETTINGS;
    }

    ServiceProvider& Application::get_service_provider()
    {
        return *_service_provider;
    }

    const ServiceProvider& Application::get_service_provider() const
    {
        return *_service_provider;
    }

    void Application::initialize()
    {
        const auto startup_begin = std::chrono::steady_clock::now();
        auto msg_coordinator = _msg_coordinator.lock();
        auto asset_manager = _asset_manager.lock();
        auto world_manager = _world_manager.lock();
        auto file_ops = _service_provider->get_service<IFileOps>().lock();

        if (!msg_coordinator || !asset_manager || !world_manager || !file_ops)
        {
            TBX_TRACE_ERROR("Application core services are unavailable during initialization.");
            _should_exit = true;
            TBX_ASSERT(
                msg_coordinator != nullptr && asset_manager != nullptr && world_manager != nullptr
                    && file_ops != nullptr,
                "Application core services are unavailable during initialization.");
            return;
        }

        try
        {
            TBX_TRACE_INFO("Initializing application: {}", _name);

#if defined(TBX_FULL_RELEASE)
            TBX_TRACE_INFO("Build Configuration: Full Release");
#elif defined(TBX_RELEASE)
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

                    if (const auto closed_event = handle_message<WindowClosedEvent>(msg))
                    {
                        auto window_manager = _window_manager.lock();
                        if (!window_manager || !window_manager->has_main_window()
                            || closed_event->get().window == window_manager->get_main_window())
                            _should_exit = true;
                    }

                    _plugin_manager.receive_message(msg);
                });

            const auto launch_config_result = read_launch_config(*file_ops);
            if (!launch_config_result.result)
            {
                _should_exit = true;
                TBX_ASSERT(false, "{}", launch_config_result.result.get_report());
                return;
            }

            const auto& launch_config = launch_config_result.config;
            _plugin_manager.load(
                file_ops->get_working_directory(),
                launch_config.plugins,
                file_ops->get_working_directory());
            _settings = load_app_settings(*asset_manager, launch_config.settings_asset);
            const auto settings_handle = _settings && _settings->id.is_valid()
                                             ? Handle(launch_config.settings_asset, _settings->id)
                                             : Handle(launch_config.settings_asset);
            if (auto reload_queue = _asset_reload_queue.lock())
            {
                reload_queue->register_handler(
                    [this, settings_handle](const AssetReloadContext& context)
                    {
                        if (!context.succeeded || context.affected_asset.id != settings_handle.id)
                        {
                            return;
                        }

                        if (const auto asset_manager = _asset_manager.lock())
                        {
                            if (auto settings = asset_manager->load<AppSettings>(settings_handle))
                            {
                                _settings = std::move(settings);
                                _name = _settings->name;
                            }
                        }
                    });
            }
            auto settings = _settings;
            _name = settings->name;

            register_app_window_manager(*_service_provider);
            register_app_physics(*_service_provider, settings->physics);
            register_app_rendering(*_service_provider);
            _input_manager = _service_provider->try_get_service<IInputManager>();
            _physics = _service_provider->try_get_service<Physics>();
            _rendering = _service_provider->try_get_service<Rendering>();
            _plugin_manager.attach_all();

            if (launch_config.startup_world.is_valid())
            {
                if (!world_manager->set_active_world(launch_config.startup_world))
                {
                    TBX_TRACE_ERROR(
                        "Failed to load startup world '{}'.",
                        launch_config.startup_world);
                    _should_exit = true;
                    return;
                }
            }

            // Open main window
            {
                auto window_manager = _service_provider->try_get_service<IWindowManager>();
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
                const auto icon_path = resolve_app_icon_path(*asset_manager, settings->icon);
                if (settings->icon.is_valid() && icon_path.empty())
                {
                    TBX_TRACE_WARNING(
                        "Failed to resolve app icon handle to a path. Window icon will not be "
                        "set.");
                }

                (void)window_manager_strong->open(
                    WindowCreateInfo {
                        .title = main_window_title,
                        .size = {1280, 720},
                        .mode = WindowMode::WINDOWED,
                        .api = settings->graphics.graphics_api,
                        .icon_path = icon_path,
                    });
            }

            // Log filesystem directories
            TBX_TRACE_INFO("Working Directory: '{}'", file_ops->get_working_directory().string());
            TBX_TRACE_INFO(
                "Logs Directory: '{}'",
                Log::get_instance().get_logs_directory().string());
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
        auto settings = _settings;
        if (!msg_coordinator || !asset_manager || !settings)
        {
            TBX_TRACE_ERROR(
                "Application update skipped because required services are unavailable.");
            _should_exit = true;
            return;
        }

        if (auto rendering = _rendering.lock())
            rendering->wait_for_pending_frame();

        // Process messages posted in the previous frame.
        msg_coordinator->flush();
        if (auto reload_queue = _asset_reload_queue.lock())
            reload_queue->flush();

        // Update delta time
        DeltaTime dt = timer.tick();
        _time_running += dt.seconds;

        // Begin update
        msg_coordinator->send<ApplicationUpdateBeginEvent>(*this, dt);

        // Physics tick
        fixed_update(dt, settings->physics);

        // Run frame systems: pump OS/window events, apply fresh device input, simulate, then
        // draw.
        {
            if (auto window_manager = _window_manager.lock())
                window_manager->update();
            if (auto input_manager = _input_manager.lock())
                input_manager->update(dt);
            _plugin_manager.update(dt);
            if (auto world_manager = _world_manager.lock())
                world_manager->update(dt, settings->world);
            if (auto script_system = _script_system.lock())
                script_system->update(dt);
            if (auto rendering = _rendering.lock())
                rendering->render(dt, settings->graphics);
        }

        // End update
        msg_coordinator->send<ApplicationUpdateEndEvent>(*this, dt);

        asset_manager->update(dt);
        ++_update_count;
    }

    void Application::fixed_update(const DeltaTime& dt, const PhysicsSettings& physics_settings)
    {
        const double fixed_step_seconds =
            std::max(0.0001, static_cast<double>(physics_settings.fixed_time_step_seconds));
        const int max_sub_steps = std::max(1, static_cast<int>(physics_settings.max_sub_steps));

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

            if (auto script_system = _script_system.lock())
                script_system->fixed_update(fixed_dt);

            if (auto physics = _physics.lock())
                physics->update(fixed_dt, physics_settings);

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
            auto asset_manager = _asset_manager.lock();
            auto thread_manager = _thread_manager.lock();
            if (!msg_coordinator || !asset_manager || !thread_manager)
                return;

            // IMPORTANT: Shutdown order matters, careful re-arranging could break things.
            TBX_TRACE_INFO("Shutting down application: {}", _name);
            TBX_TRACE_INFO(
                "Total Run Time: {:.2f}s, Total Updates: {}",
                _time_running,
                _update_count);

            // 1. Send shutdown event.
            msg_coordinator->send<ApplicationShutdownEvent>(*this);

            // 2. Release renderer-owned graphics resources while the window/context services
            // live.
            _rendering = {};
            if (_service_provider->has_service<Rendering>())
                _service_provider->deregister_service<Rendering>();

            // 3. Close all managed windows.
            if (auto window_manager = _window_manager.lock())
                window_manager->shutdown();
            _window_manager = {};
            _should_exit = true;

            // 4. Stop using physics before scripts and plugin-owned services begin teardown.
            _physics = {};

            // 5. Destroy runtime script instances before their worlds are released.
            _script_system = {};
            if (_service_provider->has_service<ScriptSystem>())
                _service_provider->deregister_service<ScriptSystem>();

            // 6. Detach plugins while their libraries are still loaded.
            _plugin_manager.detach_all();
            _input_manager = {};
            if (_service_provider->has_service<Physics>())
                _service_provider->deregister_service<Physics>();
            if (_service_provider->has_service<IWindowManager>())
                _service_provider->deregister_service<IWindowManager>();

            // 7. Unload world/entity assets after plugin teardown.
            if (auto world_manager = _world_manager.lock())
                world_manager->clear_active_world();
            _world_manager = {};
            if (_service_provider->has_service<WorldManager>())
                _service_provider->deregister_service<WorldManager>();
            if (_service_provider->has_service<AssetReloadQueue>())
                _service_provider->deregister_service<AssetReloadQueue>();
            _settings = {};
            asset_manager->unload_all();

            // 8. Unload detached plugin libraries after plugin-authored component state is
            // gone.
            _plugin_manager.unload_all();

            // 9. Stop dedicated thread lanes after plugin teardown.
            thread_manager->stop_all();

            // 10. Process any remaining posted messages and clear handlers.
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
