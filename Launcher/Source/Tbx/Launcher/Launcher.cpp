#include "Tbx/Launcher/Launcher.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Plugins/PluginFinder.h"
#include "Tbx/Plugins/PluginLoader.h"

namespace Tbx::Launcher
{
    static App CreateApp(const AppConfig& config)
    {
        // Load plugins and runtimes
        Queryable<Ref<Plugin>> plugins;
        {
            auto pluginMetas = PluginFinder(FileSystem::GetPluginDirectory(), config.Plugins).Result();
            plugins = PluginLoader(pluginMetas, EventBus::Global).Results();
        }

        // Get runtimes and separate them from the rest of the plugins
        auto runtimes = plugins.OfType<Runtime>();
        plugins = plugins.Where([&](const Ref<Plugin>& p)
        {
            return !runtimes.Any([&p](const Ref<Runtime>& r) { return r.get() == p.get(); });
        });

        return App(config.Name, config.Settings, plugins, runtimes, EventBus::Global);
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
            auto app = CreateApp(config);
            app.Run();

            status = app.Status;
            running =
                status != AppStatus::Error &&
                status != AppStatus::Closed;
        }

        return status;
    }
}
