#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Plugins/PluginFinder.h"
#include "Tbx/Plugins/PluginLoader.h"

namespace Tbx::Launcher
{
    static App CreateApp(const AppConfig& config)
    {
        // Load plugins
        Queryable<Ref<Plugin>> plugins;
        {
            auto pluginMetas = PluginFinder(FileSystem::GetPluginDirectory(), config.Plugins).Result();
            plugins = PluginLoader(pluginMetas, EventBus::Global).Results();
        }

        // Init logger
        Ref<ILogger> loggerPlugin = nullptr;
        {
            auto loggerPlugins = plugins.OfType<ILogger>();
            if (!loggerPlugins.Empty())
            {
                loggerPlugin = loggerPlugins.First();
                Log::SetLogger(loggerPlugin);
            }
        }

        // Create and run the app, this will block until the app closes
        return App(config.Name, config.Settings, plugins, EventBus::Global);
    }

    AppStatus Launch(const AppConfig& config)
    {
        auto status = AppStatus::None;
        auto running = true;

        // Reload loop, to allow for hot reloading
        // To do this (hot reload) build the code and then run it detached from VS
        // Then when you want to reload rebuild and press F2 in the app window (only available in non-release builds)
        while (running)
        {
            // Create and run the app
            {
                // Create and run the app, this will block until the app closes
                auto app = CreateApp(config);
                app.Run();

                // After we've closed check if the app is asking for a reload
                // or if we should fully shutdown
                status = app.Status;
                running =
                    status != AppStatus::Error &&
                    status != AppStatus::Closed;
            }

            // Close log after app has closed and been disposed
            Log::ClearLogger();
        }

        return status;
    }
}
