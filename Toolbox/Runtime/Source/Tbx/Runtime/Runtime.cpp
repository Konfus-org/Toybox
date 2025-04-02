#include "Tbx/Runtime/Runtime.h"
#include "Tbx/Runtime/Plugin Server/PluginServer.h"
#include <Tbx/Core/Debug/DebugAPI.h>
#include <Tbx/App/Layers/Layer.h>
#include <Tbx/App/App.h>
#include <chrono>
#include <sstream>
#include <format>

namespace Tbx
{
    void LoadPlugins(const std::string& pathToPlugins)
    {
#ifdef TBX_DEBUG

        // DEBUG:

        // Open plugins with debug/build path
        PluginServer::LoadPlugins(pathToPlugins);

        // No log file in debug
        Log::Open();

        // Once log is open, we can print out all loaded plugins to the log for debug purposes
        const auto& plugins = PluginServer::GetLoadedPlugins();
        const auto& numPlugins = plugins.size();
        TBX_INFO("Loaded {0} plugins:", numPlugins);
        for (const auto& loadedMod : plugins)
        {
            const auto& pluginInfo = loadedMod->GetPluginInfo();
            const auto& pluginName = pluginInfo.GetName();
            const auto& pluginVersion = pluginInfo.GetVersion();
            const auto& pluginAuthor = pluginInfo.GetAuthor();
            const auto& pluginDescription = pluginInfo.GetDescription();

            TBX_INFO("{0}:", pluginName);
            TBX_INFO("    - Version: {0}", pluginVersion);
            TBX_INFO("    - Author: {0}", pluginAuthor);
            TBX_INFO("    - Description: {0}", pluginDescription);
        }

        // TODO: only keep last 10 log files in release...
#else 

        // RELEASE:

        // Open plugins with release path
        PluginServer::LoadPlugins("..\\Plugins");

        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto& logPath = std::format("Logs\\{}.log", currentTime);
        Log::Open(logPath);

#endif
    }

    void UnloadPlugins()
    {
        PluginServer::Shutdown();
    }

    void LoadExternalLayers(App& app)
    {
        auto layerPlugins = PluginServer::GetPlugins<Layer>();
        for (const auto& layerPlugin : layerPlugins)
        {
            app.PushLayer(layerPlugin);
        }
    }

    void LaunchAndRun(App& app)
    {
        app.Launch();
        while (app.IsRunning())
        {
            app.Update();
        }
        app.Close();
    }

    bool Run(App& app)
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
            LoadExternalLayers(*app);
            LaunchAndRun(*app);

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