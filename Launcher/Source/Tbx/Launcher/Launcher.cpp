#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Files/Paths.h"

namespace Tbx::Launcher
{
    AppStatus Launch(
        const std::string& name,
        const Settings& settings,
		const std::vector<std::string>& args) // TODO: deal with args (just one to start --headless)
    {
        auto status = AppStatus::None;
        auto running = true;

        while (running)
        {
			// Setup required systems for the app
            auto eventBus = MakeRef<EventBus>();
            auto pluginServer = PluginServer(FileSystem::GetPluginDirectory(), eventBus);
			auto discoveredPlugins = pluginServer.GetPlugins();

            // Init logging
            Log::Initialize(discoveredPlugins.OfType<ILogger>().front());

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

			// Shutdown logging
            Log::Shutdown();
        }

        return status;
    }
}