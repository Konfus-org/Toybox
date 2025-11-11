#include "tbx/application.h"
#include "tbx/debug/macros.h"
#include "tbx/messages/commands/app_commands.h"
#include "tbx/messages/dispatcher_context.h"
#include "tbx/messages/events/app_events.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"
#include "tbx/tsl/casting.h"

namespace tbx
{
    Application::Application(AppDescription desc)
        : _desc(std::move(desc))
    {
        initialize();
    }

    Application::~Application()
    {
        shutdown();
    }

    int Application::run()
    {
        DeltaTimer timer = {};
        while (!_should_exit)
        {
            update(timer);
        }
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

    const IMessageDispatcher& Application::get_dispatcher() const
    {
        return static_cast<const IMessageDispatcher&>(_msg_coordinator);
    }

    void Application::initialize()
    {
        // Set dispatcher scope
        DispatcherScope scope(_msg_coordinator);

        // Load plugins based on description
        if (!_desc.plugins_directory.empty())
        {
            auto loaded = load_plugins(_desc.plugins_directory, _desc.requested_plugins);
            for (auto& lp : loaded)
            {
                _loaded.push_back(std::move(lp));
            }
        }

        _msg_coordinator.add_handler(
            [this](Message& msg)
            {
                handle_message(msg);
            });
        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                _msg_coordinator.add_handler(
                    [plugin = p.instance.get()](Message& msg)
                    {
                        plugin->receive_message(msg);
                    });
                p.instance->attach(*this);
            }
        }

        auto initialized = ApplicationInitializedEvent(this, _desc);
        _msg_coordinator.send(initialized);
    }

    void Application::update(DeltaTimer timer)
    {
        DispatcherScope scope(_msg_coordinator);

        // Process messages posted in previous frame
        _msg_coordinator.process();

        // Update delta time
        DeltaTime dt = timer.tick();

        ApplicationUpdateBeginEvent begin_update(this, dt);
        _msg_coordinator.send(begin_update);

        // Update all loaded plugins
        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                p.instance->update(dt);
            }
            else
            {
                TBX_TRACE_WARNING("Plugin {} is null at runtime!", p.meta.name);
            }
        }

        ApplicationUpdateEndEvent end_update(this, dt);
        _msg_coordinator.send(end_update);
    }

    void Application::shutdown()
    {
        DispatcherScope scope(_msg_coordinator);

        ApplicationShutdownEvent shutdown_event(this);
        _msg_coordinator.send(shutdown_event);

        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                p.instance->detach();
            }
        }
        _loaded.clear();
        _msg_coordinator.clear();
    }

    void Application::handle_message(const Message& msg)
    {
        if (tbx::is<ExitApplicationCommand>(&msg))
        {
            _should_exit = true;
        }
    }
}
