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
        , _filesystem(desc.working_root, {}, desc.logs_directory, {})
        , _main_window(
              _msg_coordinator,
              desc.name.empty() ? std::string("Toybox Application") : desc.name,
              {1280, 720},
              WindowMode::Windowed,
              false)
        , _settings(_msg_coordinator)
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

    IMessageHandlerRegistrar& Application::get_message_registrar()
    {
        return static_cast<IMessageHandlerRegistrar&>(_msg_coordinator);
    }

    IMessageQueue& Application::get_message_queue()
    {
        return static_cast<IMessageQueue&>(_msg_coordinator);
    }

    EntityRegistry& Application::get_entity_registry()
    {
        return _entity_registry;
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
            auto& fs = get_filesystem();
            _loaded = load_plugins(fs.get_plugins_directory(), requested_plugins, fs, *this);
            for (const auto& loaded : _loaded)
                _filesystem.add_assets_directory(loaded.meta.resource_directory);

            // Log filesystem directories
            TBX_TRACE_INFO("Working Directory: {}", _filesystem.get_working_directory().string());
            TBX_TRACE_INFO("Plugins Directory: {}", _filesystem.get_plugins_directory().string());
            TBX_TRACE_INFO("Logs Directory: {}", _filesystem.get_logs_directory().string());
            const auto& asset_roots = _filesystem.get_assets_directories();
            for (const auto& root : asset_roots)
                TBX_TRACE_INFO("Asset Directory: {}", root.string());

            // Tell everyone we're initialized
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

    void Application::update(DeltaTimer& timer)
    {
        // Process messages posted in previous frame
        _msg_coordinator.flush();

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
            _asset_manager.unload_unreferenced();
    }

    void Application::shutdown()
    {
        try
        {
            // IMPORTANT: Shutdown order matters, careful re-arranging could break things.

            GlobalDispatcherScope scope(_msg_coordinator);

            // 1. Send shutdown event
            _msg_coordinator.send<ApplicationShutdownEvent>(this);

            // 2. Close main window
            _main_window.is_open = false;
            _should_exit = true;

            // 3. Destroy all entities
            _entity_registry.destroy_all();
            _asset_manager.unload_all();

            // 4. Detach and unload all non-logging plugins first, then logging plugins.
            if (!_loaded.empty())
            {
                std::vector<LoadedPlugin> logging_plugins;
                std::vector<LoadedPlugin> non_logging_plugins;
                logging_plugins.reserve(_loaded.size());
                non_logging_plugins.reserve(_loaded.size());

                for (auto& plugin : _loaded)
                {
                    if (plugin.meta.category == PluginCategory::Logging)
                        logging_plugins.push_back(std::move(plugin));
                    else
                        non_logging_plugins.push_back(std::move(plugin));
                }

                _loaded.clear();
                non_logging_plugins.clear();
                _msg_coordinator.flush();
                logging_plugins.clear();
            }

            // 5. Process any remaining posted messages and clear handlers
            _msg_coordinator.flush();
            _msg_coordinator.clear_handlers();
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
        if (auto* exit_request = handle_message<ExitApplicationRequest>(msg))
        {
            _should_exit = true;
            exit_request->state = MessageState::Handled;
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
