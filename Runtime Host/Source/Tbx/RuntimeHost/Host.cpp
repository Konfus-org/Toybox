#include "Tbx/RuntimeHost/Host.h"
#include <Tbx/Core/Plugins/PluginServer.h>
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Runtime/Layers/Layer.h>
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/App/App.h>
#include <chrono>
#include <sstream>
#include <format>

namespace Tbx
{
    std::shared_ptr<App> Load(const std::string& pathToPlugins)
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
            TBX_ERROR("{Failed to load plugins at {0}! Error: {1}", pathToPlugins, ex.what());
            return nullptr;
        }
    }

    AppStatus Run(std::shared_ptr<App> app)
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
            TBX_ERROR("App crashed! Error: {0}", ex.what());
            return AppStatus::Error;
        }

        return app->GetStatus();
    }

    AppStatus RunHost(const std::string& pathToPlugins)
    {
        // Load app and plugins
        auto app = Load(pathToPlugins);

        // Run app
        auto status = Run(app);
        if (status == AppStatus::Reloading)
        {
            // Clear ref to app so we can unload it below
            app.reset();

            // Reload/rerun if a reload is triggered...
            PluginServer::UnloadPlugins();
            status = RunHost(pathToPlugins);
        }

        // Clear ref to app so we can unload it below
        app.reset();

        // Unload plugins
        PluginServer::UnloadPlugins();

        // Finally return and assert last status of the app
        TBX_ASSERT(status == AppStatus::Closed, "App didn't shutdown correctly!");
        return status;
    }
}