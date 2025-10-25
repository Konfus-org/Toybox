#include "tbx/application.h"
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
        DeltaTimer timer;
        DispatcherScope scope(&_dispatcher);
        while (!_should_exit)
        {
            // Process messages posted in previous frame
            _dispatcher.process();
            DeltaTime dt = timer.tick();
            for (auto& p : _loaded)
            {
                if (p.instance)
                    p.instance->on_update(dt);
            }
        }
        return 0;
    }

    void Application::request_exit()
    {
        _should_exit = true;
    }

    void Application::initialize()
    {
        // Load dynamic plugins based on description
        if (!_desc.plugins_directory.empty())
        {
            std::vector<LoadedPlugin> loaded = load_plugins(_desc.plugins_directory, _desc.requested_plugins);
            for (auto& lp : loaded)
            {
                _loaded.push_back(std::move(lp));
            }
        }

        // Attach all plugins with a basic context
        ApplicationContext ctx{};
        ctx.instance = this;
        ctx.description = _desc;

        for (auto& p : _loaded)
        {
            if (p.instance)
            {
                Plugin* target = p.instance.get();
                _dispatcher.add_handler([target](const Message& msg)
                {
                    if (!msg.is_handled)
                        target->on_message(msg);
                });
                p.instance->on_attach(ctx, _dispatcher);
            }
        }
    }

    void Application::update()
    {
    }

    void Application::shutdown()
    {
        for (auto& p : _loaded)
        {
            if (p.instance)
                p.instance->on_detach();
        }
        _loaded.clear();
        _dispatcher.clear();
    }
}
