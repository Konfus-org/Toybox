#include "Tbx/Runtime/Runtime.h"
#include "Tbx/Core/Plugins/PluginServer.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/App/Layers/Layer.h>
#include <Tbx/App/Input/Input.h>
#include <Tbx/App/App.h>
#include <chrono>
#include <sstream>
#include <format>
namespace Tbx
{
    bool RunApp(std::shared_ptr<App> app)
    {
        bool reloadPlugins = false;

        // Launch first
        app->Launch();

        // Load layer plugins
        auto layerPlugins = PluginServer::GetPlugins<Layer>();
        for (const auto& layerPlugin : layerPlugins)
        {
            app->PushLayer(layerPlugin);
        }

        // Then run the app!
        while (app->IsRunning())
        {

#ifndef TBX_RELEASE
            // Only allow reloading and force quit whwn not released!
            
            // Shortcut to kill the app
            if (Input::IsKeyDown(TBX_KEY_F4) && 
                (Input::IsKeyDown(TBX_KEY_LEFT_ALT) || Input::IsKeyDown(TBX_KEY_RIGHT_ALT)))
            {
                break;
            }

            // Shortcut to hot reload plugins
            if (Input::IsKeyDown(TBX_KEY_F5))
            {
                reloadPlugins = true;
                break;
            }
#endif

            // Update the app
            app->Update();
        }

        // Close the app
        app->Close();

        return reloadPlugins;
    }

    void RunLoadedApp()
    {
        auto apps = PluginServer::GetPlugins<App>();
        TBX_ASSERT(apps.size() != 0,
            "Toybox needs an app defined to run, have you setup an app correctly?"
            "The app library should be loaded as a tbx plugin to allow for hot reloading and you should have a host application to call 'Tbx::Run'.");
        TBX_ASSERT(!(apps.size() > 1), "Toybox only supports one app at a time!");

        auto app = apps[0];
        TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

        bool reloadPlugins = RunApp(app);

        if (reloadPlugins)
        {
            // Clear our reference to the app so that it can be reloaded
            app.reset();

            // Reload the plugin and rerun the app plugin
            PluginServer::ReloadPlugins();
            RunLoadedApp();
        }
    }

    bool Load(const std::string& pathToPlugins)
    {
        try
        {
            PluginServer::LoadPlugins(pathToPlugins);
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());
            return false;
        }
        return true;
    }

    bool Run()
    {
        try
        {
            RunLoadedApp();
            PluginServer::UnloadPlugins();
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());

            return false;
        }

        return true;
    }
}