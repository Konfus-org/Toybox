#include "tbx/application.h"
#include "tbx/debug/macros.h"
#include "tbx/memory/casting.h"
#include "tbx/messages/commands/app_commands.h"
#include "tbx/messages/dispatcher_context.h"
#include "tbx/messages/events/app_events.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"

namespace tbx
{
    Application::Application(const AppDescription& desc)
        : _desc(desc)
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

    const AppDescription& Application::get_description() const noexcept
    {
        return _desc;
    }

    IMessageDispatcher& Application::get_dispatcher() noexcept
    {
        return static_cast<IMessageDispatcher&>(_msg_coordinator);
    }

    const IMessageDispatcher& Application::get_dispatcher() const noexcept
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
            std::vector<LoadedPlugin> loaded =
                load_plugins(_desc.plugins_directory, _desc.requested_plugins);
            for (auto& lp : loaded)
            {
                _loaded.push_back(std::move(lp));
            }
        }

        // Attach all plugins with a basic context
        ApplicationContext ctx = {.instance = this, .description = _desc};

        _msg_coordinator.add_handler([this](const Message& msg) { handle_message(msg); });
        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                _msg_coordinator.add_handler(
                    [plugin = p.instance.get()](const Message& msg)
                    {
                        if (plugin)
                            plugin->on_message(msg);
                    });
                p.instance->on_attach(ctx);
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
                p.instance->on_update(dt);
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
                p.instance->on_detach();
            }
        }
        _loaded.clear();
        _msg_coordinator.clear();
    }

    void Application::handle_message(const Message& msg)
    {
        if (is<ExitApplicationCommand>(&msg))
        {
            _should_exit = true;
        }
    }
}
