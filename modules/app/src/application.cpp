#include "tbx/app/application.h"
#include "tbx/app/app_events.h"
#include "tbx/app/app_requests.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/handler.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/time/delta_time.h"

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

    void Application::initialize()
    {
        // Set dispatcher scope
        auto scope = DispatcherScope(_msg_coordinator);
        {
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
                    on_message(msg);
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

    void Application::on_message(const Message& msg)
    {
        tbx::handle_message<ExitApplicationRequest>(
            msg,
            [this](const ExitApplicationRequest&)
            {
                _should_exit = true;
            });
    }
}
