#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Plugins/PluginFinder.h"
#include "Tbx/Plugins/PluginLoader.h"

namespace Tbx::Launcher
{
    AppStatus Launch(const AppConfig& config)
    {
        auto status = AppStatus::None;
        auto running = true;

        // Reload loop, to allow for hot reloading
        // To do this (hot reload) build the code and then run it detached from VS
        // Then when you want to reload rebuild and press F2 in the app window (only available in non-release builds)
        while (running)
        {
            // Setup event bus
            auto eventBus = MakeRef<EventBus>();

            // Create and run the app
            {
                // Load plugins
                Collection<Ref<Plugin>> plugins;
                {
                    auto pluginMetas = PluginFinder(FileSystem::GetPluginDirectory(), config.Plugins).Result();
                    plugins = PluginLoader(pluginMetas, eventBus).Results();
                }

                // Init logger
                Ref<ILogger> loggerPlugin = nullptr;
                {
                    auto loggerPlugins = plugins.OfType<ILogger>();
                    if (!loggerPlugins.empty())
                    {
                        TBX_ASSERT(loggerPlugins.size() == 1, "Launcher: Only one logger plugin is allowed!");
                        loggerPlugin = loggerPlugins.front();
                        Log::SetLogger(loggerPlugin);
                    }
                }

                // Create and run the app, this will block until the app closes
                auto app = App(config.Name, config.Settings, plugins, eventBus);
                app.Run();

                // After we've closed check if the app is asking for a reload
                // or if we should fully shutdown
                status = app.Status;
                running =
                    status != AppStatus::Error &&
                    status != AppStatus::Closed;
            }

            // Last thing we do is close the log
            Log::ClearLogger();
        }

        return status;
    }
}
