#include "tbx/systems/app/application.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/graphics_backend.h"
#include "tbx/interfaces/input_manager.h"
#include "tbx/interfaces/physics_backend.h"
#include "tbx/interfaces/window_backend.h"
#include "tbx/interfaces/window_manager.h"
#include "tbx/systems/app/messages.h"
#include "tbx/systems/assets/manager.h"
#include "tbx/systems/assets/messages.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/assets/serialization_registry.h"
#include "tbx/systems/async/job_system.h"
#include "tbx/systems/debugging/logging.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/messages.h"
#include "tbx/systems/graphics/rendering.h"
#include "tbx/systems/messaging/message_coordinator.h"
#include "tbx/systems/physics/physics.h"
#include "tbx/systems/plugin_api/plugin_manager.h"
#include "tbx/systems/plugin_api/service_provider.h"
#include "tbx/systems/scripting/script_system.h"
#include "tbx/systems/time/delta_time.h"
#include "tbx/systems/windowing/manager.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <vector>

namespace tbx
{
    // Writes BGRA, top-down pixels (the layout IGraphicsBackend::read_back_buffer delivers) as a
    // 32-bit BMP. Used by the --screenshot diagnostic so a real rendered frame can be inspected
    // without a window-capture step (GDI/PrintWindow return black for hardware GL surfaces).
    static bool write_bgra_bmp(
        const std::filesystem::path& path,
        uint32 width,
        uint32 height,
        const std::vector<uint8>& bgra_top_down)
    {
        if (width == 0U || height == 0U
            || bgra_top_down.size() < static_cast<size>(width) * height * 4U)
            return false;

        const uint32 pixel_bytes = width * height * 4U;
        const uint32 file_size = 54U + pixel_bytes;
        auto put_u32 = [](uint8* out, uint32 value)
        {
            out[0] = static_cast<uint8>(value & 0xFFU);
            out[1] = static_cast<uint8>((value >> 8U) & 0xFFU);
            out[2] = static_cast<uint8>((value >> 16U) & 0xFFU);
            out[3] = static_cast<uint8>((value >> 24U) & 0xFFU);
        };

        uint8 header[54] = {};
        header[0] = 'B';
        header[1] = 'M';
        put_u32(header + 2, file_size);
        put_u32(header + 10, 54U); // pixel data offset
        put_u32(header + 14, 40U); // DIB header size
        put_u32(header + 18, width);
        // Negative height marks the rows as top-down, matching the readback's delivered order.
        put_u32(header + 22, static_cast<uint32>(-static_cast<int32>(height)));
        header[26] = 1U; // planes
        header[28] = 32U; // bits per pixel
        put_u32(header + 34, pixel_bytes);

        auto stream = std::ofstream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;
        stream.write(reinterpret_cast<const char*>(header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(bgra_top_down.data()), pixel_bytes);
        return stream.good();
    }

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
        //// INITIALIZE: BUILD CORE SERVICE INSTANCES ////

        const auto settings_path = command_list.get<std::string>("settings");
        auto startup_asset_directories = std::vector<std::filesystem::path>();
        if (!settings_path.empty())
        {
            const auto settings_parent =
                std::filesystem::path(settings_path).lexically_normal().parent_path();
            if (!settings_parent.empty())
                startup_asset_directories.push_back(settings_parent);
        }

        if (!_service_provider)
            _service_provider = std::make_shared<ServiceProvider>();

        auto file_ops = _service_provider->try_get_service<IFileOps>().lock();
        const bool has_file_ops = file_ops != nullptr;
        if (!file_ops)
            file_ops = std::make_shared<FileOperator>(root_directory);

        auto message_coordinator = _service_provider->try_get_service<IMessageCoordinator>().lock();
        const bool has_message_coordinator = message_coordinator != nullptr;
        if (!message_coordinator)
            message_coordinator = std::make_shared<MessageCoordinator>();

        auto serialization_registry =
            _service_provider->try_get_service<SerializationRegistry>().lock();
        const bool has_serialization_registry = serialization_registry != nullptr;
        if (!serialization_registry)
            serialization_registry = std::make_shared<SerializationRegistry>(file_ops);

        auto asset_manager = _service_provider->try_get_service<AssetManager>().lock();
        const bool has_asset_manager = asset_manager != nullptr;
        if (!asset_manager)
        {
            asset_manager = std::make_shared<AssetManager>(
                message_coordinator,
                serialization_registry,
                file_ops->get_working_directory(),
                std::move(startup_asset_directories),
                HandleSource(),
                file_ops);
        }

        auto world_manager = _service_provider->try_get_service<WorldManager>().lock();
        const bool has_world_manager = world_manager != nullptr;
        if (!world_manager)
            world_manager = std::make_shared<WorldManager>(asset_manager, message_coordinator);

        auto script_system = _service_provider->try_get_service<ScriptSystem>().lock();
        const bool has_script_system = script_system != nullptr;
        if (!script_system)
        {
            script_system = std::make_shared<ScriptSystem>(
                _service_provider,
                asset_manager,
                world_manager,
                message_coordinator);
        }

        auto job_system = _service_provider->try_get_service<JobSystem>().lock();
        const bool has_job_system = job_system != nullptr;
        if (!job_system)
            job_system = std::make_shared<JobSystem>();

        auto thread_manager = _service_provider->try_get_service<ThreadManager>().lock();
        const bool has_thread_manager = thread_manager != nullptr;
        if (!thread_manager)
            thread_manager = std::make_shared<ThreadManager>();

        _file_ops = file_ops;
        _msg_coordinator = message_coordinator;
        _asset_manager = asset_manager;
        _world_manager = world_manager;
        _thread_manager = thread_manager;
        _script_system = script_system;

        //// INITIALIZE: REGISTER CORE SERVICES ////

        // Register in dependency order so constructors and later lookups always see prerequisites:
        // file/message -> serialization -> asset state -> world/script -> job/thread execution.
        if (!has_file_ops)
            _service_provider->register_service<IFileOps>(file_ops);
        if (!has_message_coordinator)
            _service_provider->register_service<IMessageCoordinator>(message_coordinator);
        if (!has_serialization_registry)
            _service_provider->register_service<SerializationRegistry>(serialization_registry);
        if (!has_asset_manager)
            _service_provider->register_service<AssetManager>(asset_manager);
        if (!has_world_manager)
            _service_provider->register_service<WorldManager>(world_manager);
        if (!has_script_system)
            _service_provider->register_service<ScriptSystem>(script_system);
        if (!has_job_system)
            _service_provider->register_service<JobSystem>(job_system);
        if (!has_thread_manager)
            _service_provider->register_service<ThreadManager>(thread_manager);

        //// INITIALIZE: BIND GLOBAL STARTUP HELPERS ////

        _plugin_manager = std::make_unique<PluginManager>(_service_provider, file_ops);

        //// INITIALIZE: LOAD SETTINGS ////

        const auto startup_settings_handle =
            settings_path.empty()
                ? Handle("Settings.json")
                : Handle(std::filesystem::path(settings_path).lexically_normal().string());
        auto settings = load_app_settings(startup_settings_handle);

        //// INITIALIZE: LOAD PLUGINS ////

        const auto requested_plugins =
            resolve_plugins(settings->plugins, command_list.get_list<std::string>("load-plugins"));
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

        //// INITIALIZE: REGISTER APP-OWNED RUNTIME SERVICES ////

        const auto window_backend = _service_provider->try_get_service<IWindowBackend>();
        const auto physics_backend = _service_provider->try_get_service<IPhysicsBackend>();
        const auto graphics_backend = _service_provider->try_get_service<IGraphicsBackend>();

        // Windowing must come first because rendering depends on a live window manager, and the
        // backend usually arrives from a plugin that was just loaded above.
        if (!window_backend.expired())
        {
            auto window_manager =
                std::make_shared<WindowManager>(message_coordinator, window_backend);
            _service_provider->register_service<IWindowManager>(window_manager);
            _window_manager = window_manager;
        }

        // Physics can be created once the plugin backend exists and the core asset/world services
        // are already registered.
        if (!physics_backend.expired())
        {
            auto physics = std::make_shared<Physics>(
                physics_backend,
                asset_manager,
                world_manager,
                message_coordinator,
                settings->physics);
            _service_provider->register_service<Physics>(physics);
            _physics = _service_provider->try_get_service<Physics>();
        }

        // Rendering comes last because it needs the graphics backend plus window/thread services
        // that were established earlier in the startup sequence.
        if (!graphics_backend.expired())
        {
            auto window_manager_service = _service_provider->try_get_service<IWindowManager>();
            if (window_manager_service.expired())
            {
                TBX_TRACE_ERROR("Application requires a window service for rendering.");
                return -1;
            }

            auto rendering = std::make_shared<Rendering>(
                graphics_backend,
                asset_manager,
                thread_manager,
                window_manager_service,
                world_manager,
                message_coordinator);
            _service_provider->register_service<Rendering>(rendering);
            _rendering = rendering;

            // TODO: move to Rendering to capture and write a screenshot to an image instead of
            // making it a app responsability and have this wierd pre-render callback thing.
            // --screenshot=<path>: capture one real rendered frame to a BMP via GPU readback, then
            // exit. This is the reliable way to validate rendering headlessly — window captures
            // (GDI/PrintWindow) return black for hardware OpenGL surfaces regardless of content.
            if (command_list.has("screenshot"))
            {
                const auto screenshot_path =
                    std::filesystem::path(command_list.get<std::string>("screenshot"));
                // Warm-up frames let the world finish its synchronous first-frame asset load before
                // the first capture attempt, so the screenshot isn't of an empty scene.
                auto attempts = std::make_shared<int>(0);
                rendering->set_pre_present_callback(
                    [this, screenshot_path, attempts](
                        IGraphicsBackend& backend,
                        const RenderTarget&,
                        const Size& backbuffer_size)
                    {
                        constexpr int WARMUP_FRAMES = 8;
                        constexpr int MAX_ATTEMPTS = 240;
                        const int attempt = (*attempts)++;
                        if (attempt < WARMUP_FRAMES)
                            return;

                        auto pixels = std::vector<uint8>();
                        if (backend.read_back_buffer(backbuffer_size, pixels) && !pixels.empty())
                        {
                            if (write_bgra_bmp(
                                    screenshot_path,
                                    backbuffer_size.width,
                                    backbuffer_size.height,
                                    pixels))
                            {
                                TBX_TRACE_INFO(
                                    "Saved screenshot to '{}'.",
                                    screenshot_path.string());
                            }
                            else
                            {
                                TBX_TRACE_ERROR(
                                    "Failed to write screenshot '{}'.",
                                    screenshot_path.string());
                            }
                            request_exit();
                        }
                        else if (attempt >= MAX_ATTEMPTS)
                        {
                            TBX_TRACE_ERROR("Screenshot readback never became ready; giving up.");
                            request_exit();
                        }
                    });
            }
        }

        // Input is app-owned like windowing/physics/rendering: the InputManager (scheme/action
        // evaluation plus host injection) lives in the engine and reads raw device state from whatever
        // IInputBackend a plugin supplied. Headless apps load no input backend, so none is created.
        const auto input_backend = _service_provider->try_get_service<IInputBackend>();
        if (!input_backend.expired())
        {
            auto input_manager = std::make_shared<InputManager>(input_backend);
            _service_provider->register_service<IInputManager>(input_manager);
            _input_manager = input_manager;
        }

        //// INITIALIZE: SET APP SETTINGS ////

        _settings = std::move(settings);
        _name = _settings ? _settings->name : "Toybox App";

        //// INITIALIZE: REGISTER MESSAGE HANDLERS ////

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

                    _settings = load_app_settings(startup_settings_handle);
                    _name = _settings ? _settings->name : "Toybox App";
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

        //// INITIALIZE: FINALIZE APP STATE ////

        // Plugins attach only after the full runtime graph exists so bind/runtime hooks see the
        // final service set instead of a partial startup state.
        get_plugin_manager().attach_all();

        // The startup world is activated before the first window frame so scripts and rendering
        // start from the intended scene state.
        if (get_settings().world.startup_world.is_valid()
            && !world_manager->set_active_world(get_settings().world.startup_world))
        {
            TBX_TRACE_ERROR(
                "Failed to load startup world '{}'.",
                get_settings().world.startup_world);
            return -1;
        }

        //// INITIALIZE: OPEN MAIN WINDOW ////

        // Headless apps have no window manager at all; otherwise the main window is required.
        // Hidden apps (such as those hosted by the Toybox Studio editor) create it invisible.
        if (!_is_headless)
        {
            const auto main_window_manager = _window_manager.lock();
            if (!main_window_manager)
            {
                TBX_TRACE_ERROR("Application requires an IWindowManager service.");
                return -1;
            }

            std::filesystem::path icon_path = {};
            if (_settings->icon.is_valid())
            {
                // Resolve the icon only after settings and assets are both live so window
                // creation sees the final application branding state.
                icon_path = asset_manager->resolve_path(_settings->icon);
                if (icon_path.empty())
                {
                    TBX_TRACE_WARNING(
                        "Failed to resolve app icon handle to a path. Window icon will not be "
                        "set.");
                }
            }

            _is_hidden = command_list.has("hidden");
            const auto main_window_mode = _is_hidden ? WindowMode::HIDDEN : WindowMode::WINDOWED;
            // The window (and so the game's render size) follows the configured graphics resolution.
            main_window_manager->open(
                WindowCreateInfo {
                    .title = _name.empty() ? std::string("Toybox Application") : _name,
                    .size = get_settings().graphics.resolution,
                    .mode = main_window_mode,
                    .api = get_settings().graphics.graphics_api,
                    .icon_path = icon_path,
                });
        }

        //// INITIALIZE: REPORT RESOLVED STARTUP PATHS ////

        // Startup logging happens after the main window is opened so any failures above short
        // circuit before emitting the "ready" environment summary.
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

        //// INITIALIZE: BROADCAST READY ////

        message_coordinator->send<ApplicationInitializedEvent>(*this);
        return 0;
    }

    void Application::shutdown()
    {
        //// SHUTDOWN: CAPTURE REQUIRED SERVICES ////

        if (!_service_provider)
            return;

        const auto shutdown_begin = std::chrono::steady_clock::now();
        auto msg_coordinator = _msg_coordinator.lock();
        auto asset_manager = _asset_manager.lock();
        auto thread_manager = _thread_manager.lock();
        if (!msg_coordinator || !asset_manager || !thread_manager)
        {
            _plugin_manager = {};
            _service_provider = {};
            return;
        }

        TBX_TRACE_INFO("Shutting down application: {}", _name);
        TBX_TRACE_INFO("Total Run Time: {:.2f}s, Total Updates: {}", _time_running, _update_count);
        msg_coordinator->send<ApplicationShutdownEvent>(*this);

        //// SHUTDOWN: RELEASE FRAME-CRITICAL RUNTIME SYSTEMS ////

        // Release rendering before windowing so GPU resources are gone while the context still
        // exists.
        _rendering = {};
        if (_service_provider->has_service<Rendering>())
            _service_provider->deregister_service<Rendering>();

        // Close the main window before removing systems that may still reference it during detach.
        if (auto window_manager = _window_manager.lock())
            window_manager->shutdown();
        _window_manager = {};
        _should_exit = true;

        // Stop frame-time physics usage before plugin/script teardown begins.
        _physics = {};

        // Destroy runtime scripts while their backing worlds and assets are still available.
        _script_system = {};
        if (_service_provider->has_service<ScriptSystem>())
            _service_provider->deregister_service<ScriptSystem>();

        //// SHUTDOWN: DETACH PLUGINS BEFORE REMOVING THEIR DEPENDENCIES ////

        // Detach plugins before tearing down the services they may have consumed.
        if (_plugin_manager)
            _plugin_manager->detach_all();
        _input_manager = {};
        if (_service_provider->has_service<Physics>())
            _service_provider->deregister_service<Physics>();
        if (_service_provider->has_service<IWindowManager>())
            _service_provider->deregister_service<IWindowManager>();

        //// SHUTDOWN: CLEAR WORLD AND ASSET STATE ////

        // Clear world state before unloading assets so entity-owned handles are released first.
        if (auto world_manager = _world_manager.lock())
            world_manager->clear_active_world();
        _world_manager = {};
        if (_service_provider->has_service<WorldManager>())
            _service_provider->deregister_service<WorldManager>();
        _settings = {};
        asset_manager->unload_all();

        // Drop the process-wide serialization registries while every module is still mapped. Plugin-owned
        // records were already erased on detach via the ownership tracker, but the app module (loaded by
        // the launcher) registers serializer/asset entries at static-init with no owning plugin, so those
        // linger here. Their std::functions point into the app module's code; clearing now — before any
        // library is unloaded — keeps the registries' own static destructors from later destroying those
        // functions after the app module has been unloaded.
        clear_serialization_registrations();

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
        _service_provider = {};

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

        // Wait for the previous frame before mutating scene state so update/render never race the
        // renderer's in-flight GPU submission.
        if (const auto rendering = _rendering.lock())
            rendering->wait_for_pending_frame();

        frame_msg_coordinator->flush();

        //// UPDATE: BROADCAST FRAME START AND ADVANCE FIXED STEP ////

        _time_running += delta_time.seconds;
        frame_msg_coordinator->send<ApplicationUpdateBeginEvent>(*this, delta_time);

        // Simulation (fixed step, world, scripts) runs unless paused. A host (e.g. Studio) can pause
        // to freeze gameplay while the world keeps rendering, so an attached editor shows the scene
        // without it advancing.
        const auto should_simulate = !_is_paused;

        if (should_simulate)
            fixed_update(delta_time);

        //// UPDATE: PUMP INPUT AND RUN FRAME SIMULATION ////

        // Window/input updates happen before plugin/world/script updates so downstream systems
        // read the latest platform and device state for this frame.
        if (const auto window_manager = _window_manager.lock())
            window_manager->update();
        if (const auto input_manager = _input_manager.lock())
            input_manager->update(delta_time);

        get_plugin_manager().update(delta_time);

        // World streaming runs every frame, even in editor mode, so the scene's chunks load and stay
        // visible while not playing. Only gameplay — scripts here, fixed-step physics above — is gated
        // on play mode, so not simulating freezes behavior without unloading the world.
        if (const auto frame_world_manager = _world_manager.lock())
            frame_world_manager->update(delta_time, get_settings().world);
        if (should_simulate)
        {
            if (const auto script_system = _script_system.lock())
            {
                // Scripts are the app's own code, so their log lines carry the app's name as category.
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
        return *_service_provider;
    }

    const ServiceProvider& Application::get_service_provider() const
    {
        return *_service_provider;
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

    void Application::request_exit()
    {
        _should_exit = true;
    }

    std::vector<std::string> Application::resolve_plugins(
        const std::vector<std::string>& settings_plugins,
        const std::vector<std::string>& command_plugins)
    {
        // Command-line plugins are additive on top of the project's own list: a host (e.g. Studio)
        // injects extra plugins like the studio bridge via --load-plugins without restating the
        // project's plugins, and standalone runs that pass nothing keep exactly the settings list.
        auto resolved = settings_plugins;
        for (const auto& plugin : command_plugins)
        {
            if (std::find(resolved.begin(), resolved.end(), plugin) == resolved.end())
                resolved.push_back(plugin);
        }

        return resolved;
    }

    std::shared_ptr<AppSettings> Application::load_app_settings(const Handle& settings_handle)
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
}
