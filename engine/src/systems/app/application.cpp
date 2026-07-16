#include "tbx/systems/app/application.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/app/app_core_service_factory.h"
#include "tbx/systems/app/app_runtime_service_factory.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/app/process_ops.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_registry.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/time/delta_time.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace tbx
{
    Application::Application() = default;

    Application::~Application() noexcept = default;

    int Application::run(
        const CommandList& command_list,
        const std::filesystem::path& root_directory)
    {
        auto init_result = initialize(command_list, root_directory);
        if (init_result != 0)
        {
            TBX_ASSERT(false, "Application initialization failed!");
            shutdown();
            return init_result;
        }

        auto timer = DeltaTimer();
        while (!should_exit())
        {
            const auto dt = timer.tick();
            update(dt);
        }

        shutdown();
        return 0;
    }

    int Application::initialize(
        const CommandList& command_list,
        const std::filesystem::path& root_directory)
    {
        //// INITIALIZE: BUILD CORE SERVICES ////

        _services.initialize();

        // A startup settings file makes its own directory an asset root so sibling assets resolve.
        const auto settings_path = command_list.get<std::string>("settings");
        auto startup_asset_directories = std::vector<std::filesystem::path>();
        if (!settings_path.empty())
        {
            const auto settings_parent =
                std::filesystem::path(settings_path).lexically_normal().parent_path();
            if (!settings_parent.empty())
                startup_asset_directories.push_back(settings_parent);
        }

        // Extra asset roots passed explicitly via --register-assets=<path> (repeatable). The editor
        // points the engine at its bundled asset-viewer content this way, so previews can load a
        // shared sky/world regardless of which project is open.
        for (const auto& directory : command_list.get_list<std::string>("register-assets"))
        {
            if (!directory.empty())
                startup_asset_directories.push_back(
                    std::filesystem::path(directory).lexically_normal());
        }

        build_core_services(root_directory, std::move(startup_asset_directories));

        // The plugin manager must exist before plugins are loaded below, but its plugins attach
        // only later, once the full runtime service graph has been built.
        _plugin_manager = std::make_unique<PluginManager>(_services.shared(), _file_ops.lock());

        //// INITIALIZE: LOAD SETTINGS ////

        const auto startup_settings_handle =
            settings_path.empty()
                ? Handle("Settings.json")
                : Handle(std::filesystem::path(settings_path).lexically_normal().string());
        _settings = load_setting(startup_settings_handle);
        _name = _settings ? _settings->name : "Toybox App";

        //// INITIALIZE: LOAD PLUGINS ////

        load_plugins(command_list);

        //// INITIALIZE: REGISTER APP-OWNED RUNTIME SERVICES ////

        if (register_runtime_services(command_list) != 0)
            return -1;

        //// INITIALIZE: LOAD INPUT MAPS ////

        // Input maps load after the runtime services exist (the InputManager is one of them) and
        // before plugins attach, so scripts find data-driven schemes ready for callbacks. The
        // manager owns the feeding (and the replace-on-reapply bookkeeping); the app only hands it
        // the settings' map list.
        if (const auto input_manager = _input_manager.lock())
            if (const auto asset_manager = _asset_manager.lock())
                input_manager->apply_input_maps(*asset_manager, get_settings().input_maps);

        //// INITIALIZE: REGISTER MESSAGE HANDLERS ////

        register_message_handlers(startup_settings_handle);

        //// INITIALIZE: FINALIZE APP STATE ////

        // Plugins attach only after the full runtime graph exists so bind/runtime hooks see the
        // final service set instead of a partial startup state.
        get_plugin_manager().attach_all();

        // The startup world is activated before the first window frame so scripts and rendering
        // start from the intended world state.
        if (get_settings().world.startup_world.is_valid())
        {
            const auto world_manager = _world_manager.lock();
            if (!world_manager
                || !world_manager->open_world(get_settings().world.startup_world))
            {
                TBX_TRACE_ERROR(
                    "Failed to load startup world '{}'.",
                    get_settings().world.startup_world);
                return -1;
            }
        }

        //// INITIALIZE: OPEN MAIN WINDOW ////

        if (open_main_window(command_list) != 0)
            return -1;

        //// INITIALIZE: REPORT RESOLVED STARTUP PATHS ////

        // Startup logging happens after the main window is opened so any failures above short
        // circuit before emitting the "ready" environment summary.
        log_startup_environment(command_list);

        //// INITIALIZE: PARENT WATCHDOG ////

        start_parent_watchdog(command_list);

        //// INITIALIZE: BROADCAST READY ////

        if (const auto message_coordinator = _msg_coordinator.lock())
            message_coordinator->send<ApplicationInitializedEvent>(*this);
        return 0;
    }

    void Application::build_core_services(
        const std::filesystem::path& root_directory,
        std::vector<std::filesystem::path> startup_asset_directories)
    {
        auto factory = AppCoreServiceFactory(_services);
        const auto core = factory.create(root_directory, std::move(startup_asset_directories));

        _file_ops = core.file_ops;
        _msg_coordinator = core.message_coordinator;
        _asset_manager = core.asset_manager;
        _world_manager = core.world_manager;
        _thread_manager = core.thread_manager;
        _script_system = core.script_system;
    }

    int Application::register_runtime_services(const CommandList& command_list)
    {
        auto factory = AppRuntimeServiceFactory(_services);
        const auto runtime = factory.create(get_settings().physics);
        if (!runtime)
            return -1;

        _window_manager = runtime->window_manager;
        _physics = runtime->physics;
        _gizmos = runtime->gizmos;
        _rendering = runtime->rendering;
        _input_manager = runtime->input_manager;

        // --screenshot=<path>: capture one real rendered frame to a BMP, then exit. Rendering owns
        // the capture itself; the app only wires its completion to a graceful exit.
        if (runtime->rendering && command_list.has("screenshot"))
        {
            auto screenshot_path =
                std::filesystem::path(command_list.get<std::string>("screenshot"));
            runtime->rendering->capture_screenshot(
                std::move(screenshot_path),
                [this](bool)
                {
                    request_exit();
                });
        }

        return 0;
    }

    void Application::register_message_handlers(const Handle& startup_settings_handle)
    {
        const auto message_coordinator = _msg_coordinator.lock();
        if (!message_coordinator)
            return;

        message_coordinator->register_handler(
            [this, startup_settings_handle](Message& msg)
            {
                if (const auto reloaded = handle_message<AssetReloadedEvent>(msg))
                {
                    if (!reloaded->get().succeeded
                        || reloaded->get().affected_asset.id != startup_settings_handle.id)
                    {
                        return;
                    }

                    // Live-reload: swap in the new settings, then re-apply the fields that aren't
                    // read per-frame (window title + resolution). Everything else (world streaming,
                    // vsync, shadows, physics) is read fresh each frame from get_settings(), so it
                    // self-applies.
                    const auto previous = _settings ? *_settings : AppSettings();
                    _settings = load_setting(startup_settings_handle);
                    _name = _settings ? _settings->name : "Toybox App";
                    apply_runtime_settings(previous, get_settings());
                }

                if (auto exit_request = handle_message<ExitApplicationRequest>(msg))
                {
                    request_exit();
                    exit_request->get().state = MessageState::HANDLED;
                    return;
                }

                if (auto pause_request = handle_message<SetApplicationPausedRequest>(msg))
                {
                    set_paused(pause_request->get().is_paused);
                    pause_request->get().state = MessageState::HANDLED;
                    return;
                }

                if (auto step_request = handle_message<StepApplicationRequest>(msg))
                {
                    request_step();
                    step_request->get().state = MessageState::HANDLED;
                    return;
                }

                if (const auto closed_event = handle_message<WindowClosedEvent>(msg))
                {
                    const auto window_manager = _window_manager.lock();
                    if (!window_manager || !window_manager->has_main_window()
                        || closed_event->get().window == window_manager->get_main_window())
                    {
                        request_exit();
                    }
                }

                get_plugin_manager().receive_message(msg);
            });
    }

    void Application::load_plugins(const CommandList& command_list)
    {
        const auto file_ops = _file_ops.lock();
        if (!file_ops)
            return;

        const auto requested_plugins = resolve_plugins(
            get_settings().plugins,
            command_list.get_list<std::string>("inject-plugins"));
        const auto plugin_root_directory = file_ops->get_working_directory();

        // Headless apps are pure simulation hosts: interaction and visualization plugins are
        // never loaded, so their backends (and the services built on them) simply do not exist.
        _is_headless = command_list.has("headless");
        const auto excluded_plugin_categories =
            _is_headless
                ? std::vector<PluginCategory> {PluginCategory::INPUT, PluginCategory::RENDERING}
                : std::vector<PluginCategory>();
        _plugin_manager->load(
            plugin_root_directory,
            requested_plugins,
            plugin_root_directory,
            excluded_plugin_categories);
    }

    int Application::open_main_window(const CommandList& command_list)
    {
        // Headless apps have no window manager at all; otherwise the main window is required.
        // Hidden apps (such as those hosted by the Toybox Studio editor) create it invisible.
        if (_is_headless)
            return 0;

        const auto window_manager = _window_manager.lock();
        if (!window_manager)
        {
            TBX_TRACE_ERROR("Application requires an IWindowManager service.");
            return -1;
        }

        std::filesystem::path icon_path = {};
        if (_settings && _settings->icon.is_valid())
        {
            // Resolve the icon only after settings and assets are both live so window
            // creation sees the final application branding state.
            const auto asset_manager = _asset_manager.lock();
            icon_path = asset_manager ? asset_manager->resolve_path(_settings->icon)
                                      : std::filesystem::path();
            if (icon_path.empty())
            {
                TBX_TRACE_WARNING(
                    "Failed to resolve app icon handle to a path. Window icon will not be set.");
            }
        }

        _is_hidden = command_list.has("hidden");
        const auto main_window_mode = _is_hidden ? WindowMode::HIDDEN : WindowMode::WINDOWED;
        // The window (and so the game's render size) follows the configured graphics resolution.
        window_manager->open(
            WindowCreateInfo {
                .title = _name.empty() ? std::string("Toybox Application") : _name,
                .size = get_settings().graphics.resolution,
                .mode = main_window_mode,
                .api = get_settings().graphics.graphics_api,
                .icon_path = icon_path,
            });
        return 0;
    }

    void Application::log_startup_environment(const CommandList& command_list) const
    {
        const auto file_ops = _file_ops.lock();
        const auto asset_manager = _asset_manager.lock();
        if (!file_ops || !asset_manager)
            return;

        const auto command_line = command_list.to_string();
        TBX_TRACE_INFO("Command Line: {}", command_line.empty() ? "<none>" : command_line);
        TBX_TRACE_INFO("Working Directory: '{}'", file_ops->get_working_directory().string());
        TBX_TRACE_INFO("Logs Directory: '{}'", Log::get_instance().get_logs_directory().string());

        const auto asset_roots = asset_manager->get_directories();
        if (asset_roots.size() > 1)
        {
            TBX_TRACE_INFO("Asset Directories:");
            for (const auto& root : asset_roots)
                TBX_TRACE_INFO("    -'{}'", root.string());
        }
        else if (!asset_roots.empty())
        {
            TBX_TRACE_INFO("Asset Directory: {}", asset_roots.front().string());
        }
        else
        {
            TBX_TRACE_INFO("Asset Directory: <none>");
        }
    }

    void Application::start_parent_watchdog(const CommandList& command_list)
    {
        // --live-together-die-together=<pid>: when the launching process (e.g. the editor) dies,
        // exit too instead of lingering as an orphan. A background thread polls the parent's
        // liveness and requests a graceful exit once it is gone.
        if (!command_list.has("live-together-die-together"))
            return;

        const auto parent_pid = command_list.get<uint32>("live-together-die-together");
        TBX_TRACE_INFO("Tied to launching process {} (--live-together-die-together).", parent_pid);
        _parent_watchdog = std::jthread(
            [this, parent_pid](std::stop_token stop_token)
            {
                while (!stop_token.stop_requested())
                {
                    if (!is_process_running(parent_pid))
                    {
                        TBX_TRACE_WARNING(
                            "Launching process {} exited; shutting down.",
                            parent_pid);
                        request_exit();
                        return;
                    }

                    // Poll about once a second, but in short slices so a graceful shutdown's
                    // stop request is honored promptly rather than waiting out the interval.
                    for (int slice = 0; slice < 5 && !stop_token.stop_requested(); ++slice)
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            });
    }

    void Application::shutdown()
    {
        // Stop the parent watchdog first: on a normal shutdown the launching process is still
        // alive, so the monitor must stand down rather than race the teardown. (The std::jthread
        // destructor joins it; this just signals the loop to exit promptly.)
        _parent_watchdog.request_stop();

        //// SHUTDOWN: CAPTURE REQUIRED SERVICES ////

        if (!_services.is_valid())
            return;

        const auto shutdown_begin = std::chrono::steady_clock::now();
        auto msg_coordinator = _msg_coordinator.lock();
        auto asset_manager = _asset_manager.lock();
        auto thread_manager = _thread_manager.lock();
        if (!msg_coordinator || !asset_manager || !thread_manager)
        {
            _plugin_manager = {};
            _services.reset();
            return;
        }

        TBX_TRACE_INFO("Shutting down application: {}", _name);
        TBX_TRACE_INFO("Total Run Time: {:.2f}s, Total Updates: {}", _time_running, _update_count);
        msg_coordinator->send<ApplicationShutdownEvent>(*this);

        //// SHUTDOWN: RELEASE FRAME-CRITICAL RUNTIME SYSTEMS ////

        // Release rendering before windowing so GPU resources are gone while the context still
        // exists.
        _rendering = {};
        if (_services.get().has_service<Rendering>())
            _services.get().deregister_service<Rendering>();
        _gizmos = {};
        if (_services.get().has_service<Gizmos>())
            _services.get().deregister_service<Gizmos>();

        // Close the main window before removing systems that may still reference it during detach.
        if (auto window_manager = _window_manager.lock())
            window_manager->shutdown();
        _window_manager = {};
        _should_exit = true;

        // Stop frame-time physics usage before plugin/script teardown begins.
        _physics = {};

        // Destroy runtime scripts while their backing worlds and assets are still available.
        _script_system = {};
        if (_services.get().has_service<ScriptSystem>())
            _services.get().deregister_service<ScriptSystem>();

        //// SHUTDOWN: DETACH PLUGINS BEFORE REMOVING THEIR DEPENDENCIES ////

        // Detach plugins before tearing down the services they may have consumed.
        if (_plugin_manager)
            _plugin_manager->detach_all();

        // Destroy the engine-owned InputManager service now, while the input plugin's library is
        // still mapped. It holds a weak_ptr<IInputBackend> whose control block lives in that plugin
        // DLL; if the manager outlived plugin unload, its weak_ptr destructor would touch the freed
        // control block (a use-after-free crash on shutdown). Mirror how
        // Rendering/Physics/windowing are released before unload_all rather than leaving it to the
        // service provider's teardown.
        _input_manager = {};
        if (_services.get().has_service<InputManager>())
            _services.get().deregister_service<InputManager>();
        if (_services.get().has_service<Physics>())
            _services.get().deregister_service<Physics>();
        if (_services.get().has_service<IWindowManager>())
            _services.get().deregister_service<IWindowManager>();

        //// SHUTDOWN: CLEAR WORLD AND ASSET STATE ////

        // Clear world state before unloading assets so entity-owned handles are released first.
        if (auto world_manager = _world_manager.lock())
            world_manager->clear_active_world();
        _world_manager = {};
        if (_services.get().has_service<WorldManager>())
            _services.get().deregister_service<WorldManager>();
        _settings = {};
        asset_manager->unload_all();

        // Drop the process-wide serialization registries while every module is still mapped.
        // Plugin-owned records were already erased on detach via the ownership tracker, but the app
        // module (loaded by the launcher) registers serializer/asset entries at static-init with no
        // owning plugin, so those linger here. Their std::functions point into the app module's
        // code; clearing now — before any library is unloaded — keeps the registries' own static
        // destructors from later destroying those functions after the app module has been unloaded.
        clear_serialization_registrations();

        // Same rationale for the script registry: its plain function pointers into the app module
        // (which registers its script types at static-init with no owning plugin) must be dropped
        // before that module unloads. Plugin-owned script entries were already purged on detach by
        // the tracker.
        clear_script_registrations();

        //// SHUTDOWN: STOP REMAINING BACKGROUND WORK ////

        // Unload plugin libraries after detach so plugin code is no longer executing.
        if (_plugin_manager)
            _plugin_manager->unload_all();

        // Stop worker threads near the end once systems and plugins have stopped scheduling work.
        thread_manager->stop_all();

        //// SHUTDOWN: FLUSH FINAL MESSAGES AND DROP OWNERSHIP ////

        // Flush any final shutdown messages before discarding the coordinator and provider.
        msg_coordinator->flush();
        msg_coordinator->clear_handlers();
        _thread_manager = {};
        _asset_manager = {};
        _file_ops = {};
        _msg_coordinator = {};
        _plugin_manager = {};
        thread_manager = {};
        asset_manager = {};
        msg_coordinator = {};
        _services.reset();

        const auto shutdown_elapsed_ms = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - shutdown_begin)
                                             .count();
        TBX_TRACE_INFO("Application shutdown completed in {:.2f} ms.", shutdown_elapsed_ms);
        TBX_TRACE_FLUSH();
    }

    void Application::update(const DeltaTime& delta_time)
    {
        //// UPDATE: CAPTURE PER-FRAME SERVICES ////

        auto frame_msg_coordinator = _msg_coordinator.lock();
        auto frame_asset_manager = _asset_manager.lock();
        if (!frame_msg_coordinator || !frame_asset_manager)
        {
            TBX_TRACE_ERROR(
                "Application update skipped because required services are unavailable.");
            request_exit();
            return;
        }

        //// UPDATE: FINISH PRIOR FRAME WORK ////

        // Wait for the previous frame before mutating world state so update/render never race the
        // renderer's in-flight GPU submission.
        if (const auto rendering = _rendering.lock())
            rendering->wait_for_pending_frame();

        // New frame: clear the immediate-mode gizmo buffers before systems/scripts/plugins
        // repopulate them this frame. The external (editor RPC) batch persists across the clear.
        if (const auto gizmos = _gizmos.lock())
            gizmos->clear();

        frame_msg_coordinator->flush();

        //// UPDATE: BROADCAST FRAME START AND ADVANCE FIXED STEP ////

        _time_running += delta_time.seconds;
        frame_msg_coordinator->send<ApplicationUpdateBeginEvent>(*this, delta_time);

        // Simulation (fixed step, world, scripts) runs unless paused. A host (e.g. Studio) can
        // pause to freeze gameplay while the world keeps rendering, so an attached editor shows the
        // world without it advancing. A queued single step (StepApplicationRequest) advances
        // exactly one fixed tick while paused, for the editor's next-frame button.
        const bool stepping = _is_paused && _pending_steps > 0;
        const bool should_simulate = !_is_paused || stepping;

        if (should_simulate)
        {
            if (stepping)
            {
                // Advance one deterministic tick regardless of the real (paused) frame delta: seed
                // the accumulator to exactly one fixed step and drive fixed_update with a zero
                // delta, so its loop runs a single sub-step. Consume the queued step.
                --_pending_steps;
                _fixed_update_accumulator_seconds = std::max(
                    0.0001,
                    static_cast<double>(get_settings().physics.fixed_time_step_seconds));
                fixed_update(DeltaTime {});
            }
            else
            {
                fixed_update(delta_time);
            }
        }

        //// UPDATE: PUMP INPUT AND RUN FRAME SIMULATION ////

        // Window/input updates happen before plugin/world/script updates so downstream systems
        // read the latest platform and device state for this frame.
        if (const auto window_manager = _window_manager.lock())
            window_manager->update();
        if (const auto input_manager = _input_manager.lock())
            input_manager->update(delta_time);

        get_plugin_manager().update(delta_time);

        // World streaming runs every frame, even in editor mode, so the world's chunks load and
        // stay visible while not playing. Only gameplay — scripts here, fixed-step physics above —
        // is gated on play mode, so not simulating freezes behavior without unloading the world.
        if (const auto frame_world_manager = _world_manager.lock())
            frame_world_manager->update(delta_time, get_settings().world);
        if (should_simulate)
        {
            if (const auto script_system = _script_system.lock())
            {
                // Scripts are the app's own code, so their log lines carry the app's name as
                // category.
                TBX_LOG_CATEGORY_SCOPE(get_name());
                script_system->update(delta_time);
            }
        }
        // Every camera in the world renders into its own target; cameras without one present
        // to the main window. Headless apps never created a rendering service, so this whole
        // block naturally no-ops there.
        {
            const auto rendering = _rendering.lock();
            const auto render_window_manager = _window_manager.lock();
            const auto render_world_manager = _world_manager.lock();
            const auto active_world =
                render_world_manager ? render_world_manager->get_active_world().lock() : nullptr;
            if (rendering && active_world)
            {
                for (auto& camera_entity : active_world->get_with<Camera>())
                {
                    const auto camera_view = CameraView::from_entity(camera_entity);
                    if (!camera_view.is_valid)
                        continue;

                    auto output_target = camera_view.camera.get_render_target();
                    auto renders_to_main_window = false;
                    if (!output_target.id.is_valid())
                    {
                        if (!render_window_manager || !render_window_manager->has_main_window())
                            continue;

                        // A hidden main window (a Studio-hosted engine) is never seen, so the game
                        // view is not worth drawing into it every frame. It still hosts the shared
                        // GL context that editor-view render textures borrow, so render it once to
                        // create that context, then skip it on every later frame.
                        if (_is_hidden && _hidden_context_primed)
                            continue;

                        output_target = RenderTarget(render_window_manager->get_main_window());
                        renders_to_main_window = true;
                    }

                    rendering
                        ->render(delta_time, get_settings().graphics, camera_view, output_target);

                    if (renders_to_main_window && _is_hidden)
                        _hidden_context_primed = true;
                }
            }

            // External cameras (editor viewports / asset previews) the engine renders that are NOT
            // entities in the active world. A host plugin (Studio) registered them and updated
            // their poses/worlds earlier this frame in its update; render them after the world
            // cameras so they observe the same simulated world this frame.
            if (rendering)
                rendering->render_external_cameras(delta_time, get_settings().graphics);
        }

        //// UPDATE: BROADCAST FRAME END AND COMMIT ASSET WORK ////

        // Asset updates happen after frame logic so load/unload side effects do not invalidate
        // systems partway through simulation.
        frame_msg_coordinator->send<ApplicationUpdateEndEvent>(*this, delta_time);
        frame_asset_manager->update(delta_time);
        ++_update_count;
    }

    void Application::fixed_update(const DeltaTime& delta_time)
    {
        const auto& settings = get_settings();
        const double fixed_step_seconds =
            std::max(0.0001, static_cast<double>(settings.physics.fixed_time_step_seconds));
        const int max_sub_steps = std::max(1, static_cast<int>(settings.physics.max_sub_steps));

        auto fixed_update_accumulator_seconds =
            _fixed_update_accumulator_seconds + delta_time.seconds;
        int sub_step_count = 0;
        while (fixed_update_accumulator_seconds >= fixed_step_seconds
               && sub_step_count < max_sub_steps)
        {
            const auto fixed_dt = DeltaTime {
                .seconds = fixed_step_seconds,
                .milliseconds = fixed_step_seconds * 1000.0,
            };

            get_plugin_manager().fixed_update(fixed_dt);

            if (const auto script_system = _script_system.lock())
                script_system->fixed_update(fixed_dt);

            if (const auto physics = _physics.lock())
                physics->update(fixed_dt, settings.physics);

            fixed_update_accumulator_seconds -= fixed_step_seconds;
            ++sub_step_count;
        }

        const double max_accumulator_seconds =
            fixed_step_seconds * static_cast<double>(max_sub_steps);
        if (fixed_update_accumulator_seconds > max_accumulator_seconds)
            fixed_update_accumulator_seconds = max_accumulator_seconds;

        _fixed_update_accumulator_seconds = fixed_update_accumulator_seconds;
    }

    ServiceProvider& Application::get_service_provider()
    {
        return _services.get();
    }

    const ServiceProvider& Application::get_service_provider() const
    {
        return _services.get();
    }

    PluginManager& Application::get_plugin_manager()
    {
        return *_plugin_manager;
    }

    const PluginManager& Application::get_plugin_manager() const
    {
        return *_plugin_manager;
    }

    const AppSettings& Application::get_settings() const
    {
        static const auto DEFAULT_SETTINGS = AppSettings();
        return _settings ? *_settings : DEFAULT_SETTINGS;
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

    bool Application::should_exit() const
    {
        return _should_exit;
    }

    bool Application::is_paused() const
    {
        return _is_paused;
    }

    void Application::set_paused(bool is_paused)
    {
        _is_paused = is_paused;
    }

    void Application::request_step()
    {
        // Queue a single step; update() advances one fixed tick on the next frame even while
        // paused. An unpaused engine already simulates every frame, so this only has an effect
        // while paused.
        _pending_steps = 1;
    }

    void Application::request_exit()
    {
        _should_exit = true;
    }

    std::vector<std::string> Application::resolve_plugins(
        const std::vector<std::string>& settings_plugins,
        const std::vector<std::string>& command_plugins)
    {
        // Command-line plugins are additive on top of the project's own list: a host (e.g. Studio)
        // injects extra plugins like the studio bridge via --inject-plugins without restating the
        // project's plugins, and standalone runs that pass nothing keep exactly the settings list.
        auto resolved = settings_plugins;
        for (const auto& plugin : command_plugins)
        {
            if (std::find(resolved.begin(), resolved.end(), plugin) == resolved.end())
                resolved.push_back(plugin);
        }

        return resolved;
    }

    std::shared_ptr<AppSettings> Application::load_setting(const Handle& settings_handle)
    {
        const auto asset_manager = _asset_manager.lock();
        if (!asset_manager)
        {
            TBX_TRACE_ERROR("Application settings require an asset manager.");
            return std::make_shared<AppSettings>();
        }

        auto settings = asset_manager->load<AppSettings>(settings_handle);
        if (settings)
            return settings;

        TBX_TRACE_WARNING(
            "Failed to load application settings '{}', falling back to default settings.",
            settings_handle);
        return std::make_shared<AppSettings>();
    }

    void Application::apply_runtime_settings(
        const AppSettings& previous,
        const AppSettings& current)
    {
        // Input maps are consumed once (fed to the InputManager), so a changed map list re-feeds
        // it. This must run even without a main window (e.g. hidden/editor-hosted runs).
        if (previous.input_maps != current.input_maps)
        {
            if (const auto input_manager = _input_manager.lock())
                if (const auto asset_manager = _asset_manager.lock())
                    input_manager->apply_input_maps(*asset_manager, current.input_maps);
        }

        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return;

        const auto& window = window_manager->get_main_window();

        // The window title follows the app name.
        if (previous.name != current.name)
        {
            window_manager->set_title(
                window,
                current.name.empty() ? std::string("Toybox Application") : current.name);
        }

        // The render resolution drives the main window size (Rendering reads the live window size
        // each frame, never the setting directly), so resize the window to apply a changed
        // resolution.
        const auto& old_size = previous.graphics.resolution;
        const auto& new_size = current.graphics.resolution;
        if (new_size.width != 0U && new_size.height != 0U
            && (old_size.width != new_size.width || old_size.height != new_size.height))
        {
            window_manager->set_size(window, new_size);
        }
    }
}
