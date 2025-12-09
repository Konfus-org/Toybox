#include "tbx/app/application.h"
#include "tbx/app/app_events.h"
#include "tbx/app/app_requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/filesystem.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"
#include <utility>

namespace tbx
{
    Application::Application(AppDescription desc)
        : _desc(std::move(desc))
        , _filesystem(
              _desc.working_root,
              _desc.plugins_directory,
              _desc.logs_directory,
              _desc.assets_directory)
    {
        initialize();
    }

    Application::~Application()
    {
        shutdown();
    }

    int Application::run()
    {
        GlobalDispatcherScope scope(_msg_coordinator);

        auto timer = DeltaTimer();
        _main_window.is_open = true;

        while (!_should_exit)
            update(timer);

        return 0;
    }

    const AppDescription& Application::get_description() const
    {
        return _desc;
    }

    IMessageDispatcher& Application::get_dispatcher()
    {
        return static_cast<IMessageDispatcher&>(_msg_coordinator);
    }

    IFileSystem& Application::get_filesystem()
    {
        return _filesystem;
    }

    void Application::initialize()
    {
        GlobalDispatcherScope scope(_msg_coordinator);

        // Register internal message handler
        _msg_coordinator.add_handler(
            [this](Message& msg)
            {
                recieve_message(msg);
            });

        // Load plugins
        {
            auto& fs = get_filesystem();
            if (!_desc.plugins_directory.empty())
            {
                auto loaded = load_plugins(fs.get_plugins_directory(), _desc.requested_plugins, fs);
                for (auto& lp : loaded)
                    _loaded.push_back(std::move(lp));
            }
            for (auto& p : _loaded)
            {
                // Register plugin message handler then attach to host
                _msg_coordinator.add_handler(
                    [plugin = p.instance.get()](Message& msg)
                    {
                        plugin->receive_message(msg);
                    });
                p.instance->attach(*this);
            }
        }

        // Send initialized event
        _msg_coordinator.send<ApplicationInitializedEvent>(this, _desc);
    }

    void Application::update(DeltaTimer timer)
    {
        // Process messages posted in previous frame
        _msg_coordinator.process();

        // Update delta time
        DeltaTime dt = timer.tick();

        _msg_coordinator.send<ApplicationUpdateBeginEvent>(this, dt);

        // Update all loaded plugins
        for (auto& p : _loaded)
            p.instance->update(dt);

        _msg_coordinator.send<ApplicationUpdateEndEvent>(this, dt);
    }

    void Application::shutdown()
    {
        GlobalDispatcherScope scope(_msg_coordinator);

        _msg_coordinator.send<ApplicationShutdownEvent>(this);

        for (auto& p : _loaded)
            p.instance->detach();

        _loaded.clear();
        _msg_coordinator.clear();
    }

    void Application::recieve_message(const Message& msg)
    {
        if (on_message(
                msg,
                [this](const ExitApplicationRequest& r)
                {
                    _should_exit = true;
                }))
        {
            return;
        }
        if (on_property_changed(
                msg,
                &Window::is_open,
                [this](const PropertyChangedEvent<Window, bool>& e)
                {
                    if (e.owner == &_main_window && !e.current)
                        _should_exit = true;
                }))
        {
            return;
        }
    }
}
