#include "Tbx/Launcher/AppHost.h"
#include "Tbx/Plugin API/PluginServer.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Layers/Layer.h"
#include "Tbx/App/App.h"
#include <chrono>
#include <format>

namespace Tbx
{
    static std::shared_ptr<App> Load(const std::string& pathToPlugins)
    {
        try
        {
            PluginServer::LoadPlugins(pathToPlugins);

            auto apps = PluginServer::GetPlugins<App>();

            TBX_ASSERT(apps.size() != 0,
                "Toybox needs an app defined to run, have you setup an app correctly?"
                "The app library should be loaded as a tbx plugin to allow for hot reloading and you should have a host application to call 'Tbx::Run'.");
            TBX_ASSERT(!(apps.size() > 1), "Toybox only supports one app at a time!");

            auto app = apps[0];
            TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

            TBX_INFO("Loaded app: {0}", app->GetName());

            return app;
        }
        catch (const std::exception& ex)
        {
            std::string errMsg = ex.what();
            TBX_ERROR("Failed to load plugins at {0}! Error: {1}", pathToPlugins, errMsg);
            return nullptr;
        }
    }

    static AppStatus Run(const std::shared_ptr<App>& app)
    {
        try
        {
            // Launch first
            app->Launch();

            // Load layer plugins
            auto layerPlugins = PluginServer::GetPlugins<Layer>();
            for (const auto& layerPlugin : layerPlugins)
            {
                app->PushLayer(layerPlugin);
            }

            // Then run the app!
            while (app->GetStatus() == AppStatus::Running)
            {
                app->Update();
            }

            // Cleanup
            app->Close();
        }
        catch (const std::exception& ex)
        {
            TBX_ASSERT(false, "App crashed! Error: {0}", ex.what());
            return AppStatus::Error;
        }

        return app->GetStatus();
    }

    AppStatus RunHost(const std::string& pathToPlugins)
    {
        std::shared_ptr<App> app = nullptr;
        auto status = AppStatus::Initializing;
        bool running = true;

        while (running)
        {
            if (status == AppStatus::Reloading || status == AppStatus::Initializing)
            {
                // Load app and plugins (app is a plugin)
                app = Load(pathToPlugins);
                if (!app)
                {
                    TBX_ASSERT(false, "Failed to load an app! Is the path to plugins macro defined and point to the correct directory?");
                    return AppStatus::Error;
                }
            }

            // Run app
            status = Run(app); 
            running = status != AppStatus::Error && status != AppStatus::Closed;

            if (status != AppStatus::Restarting)
            {
                // We only want to unload everything if we are reloading things or shutting down!
                app.reset();
                PluginServer::UnloadPlugins();
            }
            else
            {
                // We are restarting, tell our plugins to restart too
                PluginServer::RestartPlugins();
            }
        }

        // Finally return and assert last status of the app
        TBX_ASSERT(status == AppStatus::Closed, "App didn't shutdown correctly!");
        return status;
    }
}