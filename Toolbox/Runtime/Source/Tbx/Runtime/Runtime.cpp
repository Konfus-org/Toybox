#include "Tbx/Runtime/Runtime.h"
#include "Tbx/Core/Plugins/PluginServer.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/App/Layers/Layer.h>
#include <Tbx/App/Input/Input.h>
#include <Tbx/App/App.h>
#include <chrono>
#include <sstream>
#include <format>
namespace Tbx
{
    void LoadPlugins(const std::string& pathToPlugins)
    {
        PluginServer::LoadPlugins(pathToPlugins);
    }

    void ReloadPlugins(const std::string& pathToPlugins)
    {
        PluginServer::ReloadPlugins(pathToPlugins);
    }

    void UnloadPlugins()
    {
        PluginServer::Shutdown();
    }

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
                app->Close();
            }

            // Shortcut to hot reload plugins
            if (Input::IsKeyDown(TBX_KEY_F5))
            {
                ReloadPlugins(TBX_PATH_TO_PLUGINS);
            }

            // Update the app
            app->Update();
        }
        app->Close();
    }

    bool Run(std::shared_ptr<App> app)
    {
        try
        {
            LoadPlugins(TBX_PATH_TO_PLUGINS);

            LoadExternalLayers(app);
            LaunchAndRun(app);

            UnloadPlugins();
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
            LoadPlugins(TBX_PATH_TO_PLUGINS);

            auto app = PluginServer::GetPlugin<App>();
            TBX_VALIDATE_PTR(app, "Could not load app! Is the TBX_PATH_TO_PLUGINS defined correctly and is the dll containing the app being build there?");

            LoadExternalLayers(app);
            LaunchAndRun(app);

            UnloadPlugins();
        }
        catch (const std::exception& ex)
        {
            TBX_ERROR("{0}", ex.what());

            return false;
        }

        return true;
    }
}