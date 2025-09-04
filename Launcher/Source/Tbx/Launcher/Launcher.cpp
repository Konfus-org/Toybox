#include "Tbx/Launcher/Launcher.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/App/App.h"
#include <chrono>
#include <format>

namespace Tbx
{
    static void LoadPlugins(const std::string& pathToPlugins)
    {
        try
        {
            // Load plugins
            PluginServer::Initialize(pathToPlugins);
        }
        catch (const std::exception& ex)
        {
            std::string errMsg = ex.what();
            TBX_TRACE_ERROR("Failed to load plugins at {0}! Error: {1}", pathToPlugins, errMsg);
        }
    }

    static std::weak_ptr<App> LoadPluginsWithAppAsPlugin(const std::string& pathToPlugins)
    {
        // Load plugins
        LoadPlugins(pathToPlugins);

        // Get and validate app
        auto apps = PluginServer::GetAllOfType<App>();
        TBX_ASSERT(apps.size() != 0,
            "Toybox needs an app defined to run, have you setup an app correctly?"
            "The app library should be loaded as a tbx plugin to allow for hot reloading and you should have a host application to call 'Tbx::Run'.");
        TBX_ASSERT(!(apps.size() > 1), "Toybox only supports one app at a time!");
        auto app = apps[0];
        TBX_VALIDATE_WEAK_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");
        TBX_TRACE_INFO("Loaded app: {0}", app.lock()->GetName());

        // Finally return our app
        return app;
    }

    static AppStatus Run(std::weak_ptr<App> app)
    {
        try
        {
            // Launch first
            app.lock()->Launch();

            // Load layer plugins
            auto layerPlugins = PluginServer::GetAllOfType<Layer>();
            for (const auto& layerPlugin : layerPlugins)
            {
                app.lock()->PushLayer(layerPlugin);
            }

            // Then run the app!
            while (app.lock()->GetStatus() == AppStatus::Running)
            {
                app.lock()->Update();
            }

            // Cleanup
            app.lock()->Close();
        }
        catch (const std::exception& ex)
        {
            TBX_ASSERT(false, "App crashed! Error: {0}", ex.what());
            return AppStatus::Error;
        }

        return app.lock()->GetStatus();
    }

    static AppStatus Launch(std::weak_ptr<App> app, const std::string& path, bool loadAppAsPlugin)
    {
        // Get plugin path
        auto plugPath = path;
        if (plugPath.empty()) plugPath = Tbx::FileSystem::GetWorkingDirectory();

        // Reload loop
        auto status = AppStatus::Initializing;
        auto running = true;
        while (running)
        {
            if (status == AppStatus::Reloading || status == AppStatus::Initializing)
            {
                if (loadAppAsPlugin)
                {
                    app = LoadPluginsWithAppAsPlugin(plugPath);
                    if (app.expired() || app.lock() == nullptr)
                    {
                        TBX_ASSERT(false, "Failed to load an app! Is the path to plugins macro defined and point to the correct directory?");
                        return AppStatus::Error;
                    }
                }
                else LoadPlugins(plugPath);
            }

            // Run app
            status = Run(app);
            running = status != AppStatus::Error && status != AppStatus::Closed;

            // Check if we should restart/reload
            if (status == AppStatus::Reloading)
            {
                // If we are restarting we need to shutdown our plugin server
                PluginServer::Shutdown();
            }
        }

        // Assert last status of the app
        TBX_ASSERT(status == AppStatus::Closed, "App didn't shutdown correctly!");

        // If we aren't running for any reason, shutdown our plugins and close the log
        PluginServer::Shutdown();

        return status;
    }

    AppStatus Launch(std::weak_ptr<App> app, const std::string& path)
    {
        return Launch(app, path, false);
    }

    AppStatus Launch(const std::string& path)
    {
        return Launch({}, path, true);
    }
}