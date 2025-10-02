#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Events/PluginEvents.h"


namespace Tbx::Launcher
{
    AppStatus Launch(
        const std::string& name,
        const AppSettings& settings,
        const std::vector<std::string>& args) // TODO: deal with args (just one to start --headless)
    {
        auto logListener = EventListener();
        auto status = AppStatus::None;
        auto running = true;

        // Main runtime loop, will reload the app if requested
        while (running)
        {
            // Setup required systems for the app
            auto eventBus = MakeRef<EventBus>();
            auto pluginServer = PluginServer(FileSystem::GetPluginDirectory(), eventBus);
            auto discoveredPlugins = pluginServer.GetPlugins();

            // Init logging
            logListener.Bind(eventBus);
            logListener.Listen<PluginLoadedEvent>([](const PluginLoadedEvent& e)
            {
                if (auto loggerPlug = std::dynamic_pointer_cast<ILogger>(e.GetLoadedPlugin()))
                {
                    Log::Initialize(loggerPlug);
                }
            });
            logListener.Listen<PluginUnloadedEvent>([](const PluginUnloadedEvent& e)
            {
                // TODO: fix - log isn't shutdown in time before logger is unloaded, causing exceptions
                if (std::dynamic_pointer_cast<ILogger>(e.GetUnloadedPlugin()))
                {
                    Log::Shutdown();
                }
            });

            // Create the app
            auto app = App(name, settings, discoveredPlugins, eventBus);

            // Run the app, this will block until the app closes
            app.Run();

            // After we've closed check if the app is asking for a reload
            // or if we should fully shutdown
            status = app.GetStatus();
            running =
                status != AppStatus::Error &&
                status != AppStatus::Closed;
        }

        return status;
    }
}