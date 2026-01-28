#include "tbx/app/application.h"
#include "tbx/app/app_events.h"
#include "tbx/app/app_requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/filesystem.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"

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
        , _filesystem(
              desc.working_root,
              desc.plugins_directory,
              desc.logs_directory,
              desc.assets_directory)
        , _main_window(
              _msg_coordinator,
              desc.name.empty() ? std::string("Toybox Application") : desc.name,
              {1280, 720},
              WindowMode::Windowed,
              false)
        , _settings(_msg_coordinator, true, GraphicsApi::OpenGL, {0, 0})
        , _asset_manager(_filesystem)
    {
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

    IMessageDispatcher& Application::get_dispatcher()
    {
        return static_cast<IMessageDispatcher&>(_msg_coordinator);
    }

    ECS& Application::get_ecs()
    {
        return _ecs;
    }

    IFileSystem& Application::get_filesystem()
    {
        return _filesystem;
    }

    AssetManager& Application::get_asset_manager()
    {
        return _asset_manager;
    }

    void Application::initialize(const std::vector<std::string>& requested_plugins)
    {
        try
        {
            GlobalDispatcherScope scope(_msg_coordinator);

            // Log app settings and configuration
            TBX_TRACE_INFO("Initializing application: {}", _name);
            TBX_TRACE_INFO("Working Directory: {}", _filesystem.get_working_directory().string());
            TBX_TRACE_INFO("Plugins Directory: {}", _filesystem.get_plugins_directory().string());
            TBX_TRACE_INFO("Logs Directory: {}", _filesystem.get_logs_directory().string());
            TBX_TRACE_INFO("Assets Directory: {}", _filesystem.get_assets_directory().string());

            // Register internal message handler
            _msg_coordinator.add_handler(
                [this](Message& msg)
                {
                    recieve_message(msg);
                });

            // Load plugins
            auto& fs = get_filesystem();
            auto plug_loader = PluginLoader();
            _loaded = plug_loader.load_plugins(fs.get_plugins_directory(), requested_plugins, fs);

            // Log loaded plugins
            for (auto& p : _loaded)
                TBX_TRACE_INFO("Loaded plugin: {} v{}", p.meta.name, p.meta.version);

            // Register plugin message handlers then attach them to host
            for (auto& p : _loaded)
            {
                _msg_coordinator.add_handler(
                    [plugin = p.instance.get()](Message& msg)
                    {
                        plugin->receive_message(msg);
                    });
                p.instance->attach(*this);
            }

            // Send initialized event
            _msg_coordinator.send<ApplicationInitializedEvent>(this);
        }
        catch (const std::exception& ex)
        {
            TBX_ASSERT(false, "Exception during application initialization: {}", ex.what());
        }
        catch (...)
        {
            TBX_ASSERT(false, "Unknown exception during application initialization.");
        }
    }

    void Application::update(DeltaTimer timer)
    {
        // Process messages posted in previous frame
        _msg_coordinator.process();

        // Update delta time
        DeltaTime dt = timer.tick();

        // Begin update
        _msg_coordinator.send<ApplicationUpdateBeginEvent>(this, dt);

        // Update all loaded plugins
        for (auto& p : _loaded)
            p.instance->update(dt);

        // End update
        _msg_coordinator.send<ApplicationUpdateEndEvent>(this, dt);

        ++_update_count;
        if ((_update_count % 5) == 0)
            _asset_manager.clean();
    }

    void Application::shutdown()
    {
        try
        {
            GlobalDispatcherScope scope(_msg_coordinator);

            _msg_coordinator.send<ApplicationShutdownEvent>(this);

            for (auto& plugin : _loaded)
            {
                plugin.instance->detach();
            }

            _ecs = {};
            _loaded.clear();
            _msg_coordinator.process();
            _msg_coordinator.clear();
            _main_window.is_open = false;
            _should_exit = true;
        }
        catch (const std::exception& ex)
        {
            TBX_ASSERT(false, "Exception during application shutdown: {}", ex.what());
        }
        catch (...)
        {
            TBX_ASSERT(false, "Unknown exception during application shutdown.");
        }
    }

    void Application::recieve_message(Message& msg)
    {
        on_message(
            msg,
            [this](ExitApplicationRequest& request)
            {
                _should_exit = true;
                request.state = MessageState::Handled;
            });
    }

}
