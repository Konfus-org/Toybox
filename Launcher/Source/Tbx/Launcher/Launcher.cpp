#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Plugins/PluginFinder.h"
#include <utility>

namespace Tbx::Launcher
{
    AppStatus Launch(const AppConfig& config)
    {
        auto status = AppStatus::None;
        auto running = true;

        // Main runtime loop, will reload the app if requested
        while (running)
        {
            // Setup required systems for the app
            auto eventBus = MakeRef<EventBus>();
            auto pluginMetas = PluginFinder(FileSystem::GetPluginDirectory(), config.Plugins).Result();
            auto pluginCache = PluginLoader(std::move(pluginMetas), eventBus).Results();

            Ref<ILogger> loggerPlugin = nullptr;
            {
                auto loggerPlugins = pluginCache.OfType<ILogger>();
                if (!loggerPlugins.empty())
                {
                    TBX_ASSERT(loggerPlugins.size() == 1, "Launcher: Only one logger plugin is allowed!");
                    loggerPlugin = loggerPlugins.front();
                    Log::Initialize(loggerPlugin);
                }
            }

            // Create the app
            {
                auto app = App(config.Name, config.Settings, std::move(pluginCache), eventBus);

                // Run the app, this will block until the app closes
                app.Run();

                // After we've closed check if the app is asking for a reload
                // or if we should fully shutdown
                status = app.Status;
                running =
                    status != AppStatus::Error &&
                    status != AppStatus::Closed;
            }

            Log::Shutdown();
        }

        return status;
    }
}