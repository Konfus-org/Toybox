#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Plugins/PluginServer.h"
#include <filesystem>

namespace Tbx
{
    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins;

    void PluginServer::RegisterPlugin(std::shared_ptr<LoadedPlugin> plugin)
    {
        _loadedPlugins.push_back(plugin);
    }

    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::GetLoadedPlugins()
    {
        return _loadedPlugins;
    }

    void PluginServer::LoadPlugins(const std::string& pathToPlugins)
    {
        const auto& pluginsInPluginDir = std::filesystem::directory_iterator(pathToPlugins);
        for (const auto& entry : pluginsInPluginDir)
        {
            // Recursively load directories
            if (entry.is_directory()) LoadPlugins(entry.path().string());
            
            // Skip anything that isn't a file
            if (!entry.is_regular_file()) continue;

            // Extension check
            if (entry.path().extension() == ".plugin")
            {
                const std::string& fileName = entry.path().filename().string();
                auto plug = std::make_shared<LoadedPlugin>(pathToPlugins, fileName);
                if (plug->IsValid() == false) continue;
                _loadedPlugins.push_back(plug);
            }
        }
    }

    void PluginServer::ReloadPlugins(const std::string& pathToPlugins)
    {
        _loadedPlugins.clear();
        LoadPlugins(pathToPlugins);
    }

    void PluginServer::Shutdown()
    {
        // Clear refs to loaded plugins.. 
        // this will cause them to unload themselves when all refs to them die
        _loadedPlugins.clear();
    }
}