#include "Tbx/PCH.h"
#include "Tbx/Plugin API/PluginServer.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include <filesystem>

namespace Tbx
{
    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins = {};
    std::string PluginServer::_pathToLoadedPlugins = "";

    void PluginServer::LoadPlugins(const std::string& pathToPlugins)
    {
        _pathToLoadedPlugins = pathToPlugins;
        TBX_ASSERT(!pathToPlugins.empty(), "Path to plugins is empty!");

        // Find plugin infos
        auto foundPluginInfos = FindPluginInfosInDirectory(pathToPlugins);

        // Sort by priority
        std::sort(foundPluginInfos.begin(), foundPluginInfos.end(), [](const PluginInfo& a, const PluginInfo& b) 
        {
            return a.GetPriority() > b.GetPriority(); 
        });

        // Load plugins
        for (const auto& pluginInfo : foundPluginInfos)
        {
            auto loadedPlugin = std::make_shared<LoadedPlugin>(pluginInfo);
            TBX_ASSERT(loadedPlugin->IsValid(), "Failed to load plugin: {0}", pluginInfo.GetName());
            if (!loadedPlugin->IsValid()) continue;
            _loadedPlugins.push_back(loadedPlugin);

            auto pluginLoadedEvent = PluginLoadedEvent(loadedPlugin);
            EventCoordinator::Send(pluginLoadedEvent);
        }

        // Log loaded plugins
        const auto& plugins = _loadedPlugins;
        const auto& numPlugins = plugins.size();
        TBX_TRACE_INFO("Loaded {0} plugins:", numPlugins);
        for (const auto& loadedPlug : plugins)
        {
            const auto& pluginInfo = loadedPlug->GetInfo();
            const auto& pluginName = pluginInfo.GetName();
            const auto& pluginVersion = pluginInfo.GetVersion();
            const auto& pluginAuthor = pluginInfo.GetAuthor();
            const auto& pluginDescription = pluginInfo.GetDescription();

            TBX_TRACE_INFO("{0}:", pluginName);
            TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
            TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
            TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
        }
    }

    void PluginServer::RestartPlugins()
    {
        TBX_TRACE_INFO("Restarting plugins...");

        // Sort by reverse priority so highest prio is unloaded last
        std::ranges::sort(_loadedPlugins, [](const std::shared_ptr<LoadedPlugin> a, const std::shared_ptr<LoadedPlugin> b)
        {
            return a->GetInfo().GetPriority() > b->GetInfo().GetPriority();
        });

        const auto& plugins = _loadedPlugins;
        for (const auto& loadedPlug : plugins)
        {
            const auto& pluginInfo = loadedPlug->GetInfo();
            const auto& pluginName = pluginInfo.GetName();
            loadedPlug->Restart();

            TBX_TRACE_INFO("Restarting: {0}", pluginName);
        }

        // Put back into reg priority
        std::ranges::sort(_loadedPlugins, [](const std::shared_ptr<LoadedPlugin> a, const std::shared_ptr<LoadedPlugin> b)
        {
            return a->GetInfo().GetPriority() > b->GetInfo().GetPriority();
        });

        TBX_TRACE_INFO("Plugins restarted!");
    }

    void PluginServer::ReloadPlugins()
    {
        TBX_TRACE_INFO("Reloading plugins...");

        // Unload...
        UnloadPlugins();
        // Reload...
        LoadPlugins(_pathToLoadedPlugins);

        TBX_TRACE_INFO("Plugins reloaded!");
    }

    void PluginServer::UnloadPlugins()
    {
        // Sort by reverse priority so highest prio is unloaded last
        std::ranges::sort(_loadedPlugins,[](const std::shared_ptr<LoadedPlugin> a, const std::shared_ptr<LoadedPlugin> b)
        {
            return a->GetInfo().GetPriority() < b->GetInfo().GetPriority();
        });

        // Clear refs to loaded plugins.. 
        // this will cause them to unload themselves when all refs to them die
        _loadedPlugins.clear();
    }

    void PluginServer::RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin)
    {
        _loadedPlugins.push_back(plugin);
    }

    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::GetLoadedPlugins()
    {
        return _loadedPlugins;
    }

    std::vector<PluginInfo> PluginServer::FindPluginInfosInDirectory(const std::string& pathToPlugins)
    {
        std::vector<PluginInfo> foundPluginInfos = {};

        const auto& filesInPluginDir = std::filesystem::directory_iterator(pathToPlugins);
        for (const auto& entry : filesInPluginDir)
        {
            // Recursively search directories
            if (entry.is_directory())
            {
                auto pluginInfosFoundInDir = FindPluginInfosInDirectory(entry.path().string());
                for (const auto& pluginInfo : pluginInfosFoundInDir)
                {
                    foundPluginInfos.push_back(pluginInfo);
                }
            }

            // Skip anything that isn't a file
            if (!entry.is_regular_file()) continue;

            // Extension check
            if (entry.path().extension() == ".plugin")
            {
                const std::string& fileName = entry.path().filename().string();

                auto plugInfo = PluginInfo(entry.path().parent_path().string(), fileName);
                TBX_ASSERT(plugInfo.IsValid(), "Invalid plugin info at: {0}!", entry.path().string());
                if (!plugInfo.IsValid()) continue;

                foundPluginInfos.push_back(plugInfo);
            }
        }

        return foundPluginInfos;
    }
}