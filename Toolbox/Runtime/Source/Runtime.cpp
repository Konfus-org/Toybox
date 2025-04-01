#include "Tbx/Runtime/Plugins/PluginServer.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/App/App.h"
#include <chrono>
#include <format>

namespace Tbx
{
    void Run(App& app)
    {
        // Load plugins
#ifdef TBX_DEBUG

        // DEBUG:
        
        // Open plugins with debug/build path
        PluginServer::LoadPlugins("..\\Build\\bin\\Plugins");

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
        // Launch and run application
        app.Launch();
        while (app.IsRunning()) app.Update();
        app.Close();

        // Has to be last! 
        // Everything depends on plugins, including the log, input and rendering. 
        // So they cannot be shutdown after plugins are unloaded.
        PluginServer::Shutdown();
    }
}

int main()
{
    try
    {
        auto appPlugin = Tbx::PluginServer::GetPlugin<Tbx::App>();
        Tbx::Run(*appPlugin);
    }
    catch (const std::exception& ex)
    {
        TBX_ERROR("{0}", ex.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}