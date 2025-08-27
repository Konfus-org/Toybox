#include "Tbx/PCH.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include <filesystem>
#include <algorithm>

namespace Tbx
{
    static bool IsTypeLoaded(const std::string& typeName)
    {
        if (typeName == "ILoggerFactoryPlugin")
            return !PluginServer::Get<ILoggerFactoryPlugin>().expired();
        if (typeName == "IRendererFactoryPlugin")
            return !PluginServer::Get<IRendererFactoryPlugin>().expired();
        if (typeName == "IWindowFactoryPlugin")
            return !PluginServer::Get<IWindowFactoryPlugin>().expired();
        if (typeName == "IInputHandlerPlugin")
            return !PluginServer::Get<IInputHandlerPlugin>().expired();
        if (typeName == "ITextureLoaderPlugin")
            return !PluginServer::Get<ITextureLoaderPlugin>().expired();
        if (typeName == "IShaderLoaderPlugin")
            return !PluginServer::Get<IShaderLoaderPlugin>().expired();
        if (typeName == "ILayerPlugin")
            return !PluginServer::Get<ILayerPlugin>().expired();

        return false;
    }

    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins = {};
    std::string PluginServer::_pathToLoadedPlugins = "";

    void PluginServer::Initialize(const std::string& pathToPlugins)
    {
        _pathToLoadedPlugins = pathToPlugins;
        TBX_ASSERT(!pathToPlugins.empty(), "Path to plugins is empty!");

        // Find plugin infos
        auto remainingPluginInfos = FindPluginInfosInDirectory(pathToPlugins);

        // Load plugins respecting dependencies
        while (!remainingPluginInfos.empty())
        {
            bool progress = false;

            for (auto it = remainingPluginInfos.begin(); it != remainingPluginInfos.end();)
            {
                const auto& info = *it;

                bool depsMet = true;

                for (const auto& dep : info.GetDependencies())
                {
                    auto found = std::find_if(_loadedPlugins.begin(), _loadedPlugins.end(), [&](const auto& plug)
                    {
                        return plug->GetInfo().GetName() == dep;
                    });

                    if (found == _loadedPlugins.end() && !IsTypeLoaded(dep))
                    {
                        depsMet = false;
                        break;
                    }
                }

                if (!depsMet)
                {
                    ++it;
                    continue;
                }

                auto loadedPlugin = std::make_shared<LoadedPlugin>(info);

                if (!loadedPlugin->IsValid())
                {
                    TBX_ASSERT(false, "Failed to load plugin: {0}", info.GetName());
#ifdef TBX_DEBUG
                    // TODO prompt to remove potentially stale plugins
                    //std::remove(loadedPlugin->GetInfo().GetFilePath().c_str());
#endif
                }

                if (loadedPlugin->IsValid())
                {
                    _loadedPlugins.push_back(loadedPlugin);

                    auto pluginLoadedEvent = PluginLoadedEvent(loadedPlugin);
                    EventCoordinator::Send(pluginLoadedEvent);
                }

                it = remainingPluginInfos.erase(it);
                progress = true;
            }

            TBX_ASSERT(progress, "Unable to resolve plugin dependencies!");
            if (!progress) break;
        }
    }

    void PluginServer::Shutdown()
    {
        TBX_TRACE_INFO("Unloading plugins...");

        // Clear refs to loaded plugins.. 
        // this will cause them to unload themselves
        // Unload one by one, reverse load order
        while (!_loadedPlugins.empty())
        {
            auto plugin = _loadedPlugins.back();
            TBX_TRACE_INFO("Unloading plugin: {}", plugin->GetInfo().GetName());

            // erase last element
            _loadedPlugins.pop_back();

            // when the shared_ptr ref count drops to 0, the plugin will unload itself
            plugin.reset();
        }
    }

    void PluginServer::ReloadPlugins()
    {
        TBX_TRACE_INFO("Reloading plugins...");

        // Unload...
        Shutdown();
        // Reload...
        Initialize(_pathToLoadedPlugins);
    }

    void PluginServer::RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin)
    {
        _loadedPlugins.push_back(plugin);
    }

    const std::vector<std::shared_ptr<LoadedPlugin>>& PluginServer::GetAll()
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