#include "tbx/application.h"
#include "tbx/debug/log_macros.h"
#include "tbx/memory/casting.h"
#include "tbx/messages/commands/app_commands.h"
#include "tbx/messages/dispatcher_context.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/time/delta_time.h"

namespace tbx
{
    Application::Application(const AppDescription& desc)
    {
        _desc = desc;
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

    MessageCoordinator& Application::get_dispatcher() noexcept
    {
        return _msg_coordinator;
    }

    const MessageCoordinator& Application::get_dispatcher() const noexcept
    {
        return _msg_coordinator;
    }

    void Application::initialize()
    {
        // Set dispatcher scope
        DispatcherScope scope(_msg_coordinator);

        // Load dynamic plugins based on description
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
                p.instance->set_host(this);
                p.instance->on_attach(ctx);
            }
        }
    }

    void Application::update(DeltaTimer timer)
    {
        DispatcherScope scope(_msg_coordinator);

        // Process messages posted in previous frame
        _msg_coordinator.process();

        // Update delta time
        DeltaTime dt = timer.tick();

        // Update all loaded plugins
        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                p.instance->on_update(dt);
            }
            else
            {
                TBX_TRACE_WARNING("Plugin {} is null at runtime!", p.meta.id);
            }
        }
    }

    void Application::shutdown()
    {
        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                p.instance->on_detach();
                p.instance->set_host(nullptr);
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
