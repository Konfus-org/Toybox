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
    void LoadExternalLayers(std::shared_ptr<App> app)
    {
        auto layerPlugins = PluginServer::GetPlugins<Layer>();
        for (const auto& layerPlugin : layerPlugins)
        {
            app->PushLayer(layerPlugin);
        }
    }

    void LaunchAndRun(std::shared_ptr<App> app)
    {
        app->Launch();
        while (app->IsRunning())
        {
            // Shortcut to kill the app
            if (Input::IsKeyDown(TBX_KEY_F4) && 
                (Input::IsKeyDown(TBX_KEY_LEFT_ALT) || Input::IsKeyDown(TBX_KEY_RIGHT_ALT)))
            {
                break;
            }

            // Shortcut to hot reload plugins
            if (Input::IsKeyDown(TBX_KEY_F5))
            {
                PluginServer::ReloadPlugins(TBX_PATH_TO_PLUGINS);
            }

            // Update the app
            app->Update();
        }

        // Close the app
        app->Close();
    }

    bool Run(std::shared_ptr<App> app)
    {
        try
        {
            PluginServer::LoadPlugins(TBX_PATH_TO_PLUGINS);

            LoadExternalLayers(app);
            LaunchAndRun(app);

            PluginServer::UnloadPlugins();
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());

            return false;
        }

        return true;
    }

    bool LoadAndRunApp()
    {
        try
        {
            PluginServer::LoadPlugins(TBX_PATH_TO_PLUGINS);

            // App lifetime, we need our ref to the app to be cleared by the time plugins are unloaded since it is a plugin
            {
                auto app = PluginServer::GetPlugin<App>();
                TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

                LoadExternalLayers(app);
                LaunchAndRun(app);
            }

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