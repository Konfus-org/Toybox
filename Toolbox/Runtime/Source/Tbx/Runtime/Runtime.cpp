#include "Tbx/Runtime/Runtime.h"
#include "Tbx/Core/Plugins/PluginServer.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/Core/Events/EventDispatcher.h>
#include <Tbx/App/Layers/Layer.h>
#include <Tbx/App/Input/Input.h>
#include <Tbx/App/App.h>
#include <chrono>
#include <sstream>
#include <format>
namespace Tbx
{
    static bool _reloadPlugins = false;

    void RunApp(std::shared_ptr<App> app)
    {
        auto layerPlugins = PluginServer::GetPlugins<Layer>();
        for (const auto& layerPlugin : layerPlugins)
        {
            app->PushLayer(layerPlugin);
        }

        app->Launch();
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
                _reloadPlugins = true;
                break;
            }
#endif

            // Update the app
            app->Update();
        }

        // Close the app
        app->Close();
    }

    void RunLoadedApp()
    {
        auto app = PluginServer::GetPlugin<App>();
        TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

        RunApp(app);

        if (_reloadPlugins)
        {
            _reloadPlugins = false;

            // Clear our reference to the app so that it can be reloaded
            app.reset();

            // Reload the plugin and rerun the app plugin
            PluginServer::ReloadPlugins(TBX_PATH_TO_PLUGINS);
            RunLoadedApp();
        }
    }

    bool Run()
    {
        try
        {
            PluginServer::LoadPlugins(TBX_PATH_TO_PLUGINS);
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