#include "Tbx/PCH.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/PluginAPI/PluginUtils.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace Tbx
{
    static std::shared_ptr<LoadedPlugin> _loggingPlugin = nullptr;

    std::vector<std::shared_ptr<LoadedPlugin>> PluginServer::_loadedPlugins = {};
    std::string PluginServer::_pathToLoadedPlugins = "";

    void PluginServer::Initialize(const std::string& pathToPlugins)
    {
        TBX_TRACE_INFO("Loading plugins...");

        _pathToLoadedPlugins = pathToPlugins;
        TBX_ASSERT(!pathToPlugins.empty(), "Path to plugins is empty!");

        // 1) Discover all plugin infos
        auto allInfos = SearchDirectoryForInfos(pathToPlugins);

        // 2) Partition: "All" plugins are deferred to the end
        std::vector<PluginInfo> normal, allLast;
        normal.reserve(allInfos.size());
        for (auto& pi : allInfos)
        {
            if (Plugin::ImplementsType(pi, "All"))
            {
                allLast.push_back(std::move(pi));
            }
            else
            {
                normal.push_back(std::move(pi));
            }
        }

        // Stable, nice-to-read order inside each bucket
        std::stable_sort(normal.begin(), normal.end(), Plugin::SortKey);
        std::stable_sort(allLast.begin(), allLast.end(), Plugin::SortKey);

        // 3) Load logging and open our log
        std::unordered_set<std::string> loadedNames;
        std::unordered_set<std::string> loadedTypes;
        auto it = std::find_if(normal.begin(), normal.end(), [](const PluginInfo& pi) { return Plugin::ImplementsType(pi, "Logger Factory"); });
        if (it != normal.end())
        {
            Plugin::Load(*it, _loadedPlugins, loadedNames, loadedTypes);
            _loggingPlugin = _loadedPlugins.back();
            normal.erase(it);
        }
        Log::Open("Tbx");
        Plugin::ReportInfo(_loggingPlugin->GetInfo());

        // 4) Resolve a bucket with dependency scheduling
        auto resolveBucket = [&](std::vector<PluginInfo>& bucket)
        {
            while (!bucket.empty())
            {
                bool progress = false;

                // Iterate and grab everything that’s ready this round
                for (auto it = bucket.begin(); it != bucket.end(); )
                {
                    if (Plugin::AreDependenciesSatisfied(*it, loadedNames, loadedTypes))
                    {
                        Plugin::Load(*it, _loadedPlugins, loadedNames, loadedTypes);
                        Plugin::ReportInfo(*it);
                        it = bucket.erase(it);
                        progress = true;
                    }
                    else
                    {
                        ++it;
                    }
                }

                if (!progress)
                {
                    Plugin::ReportUnresolvedDependencies(bucket, loadedNames, loadedTypes);
                    TBX_ASSERT(false, "Unable to resolve plugin dependencies!");
                    break;
                }
            }
        };

        // 5) Load “normal” first, then the ones explicitly marked to wait until the end
        resolveBucket(normal);
        resolveBucket(allLast);
    }

    void PluginServer::Shutdown()
    {
        TBX_TRACE_INFO("Unloading plugins...");

        // Clear refs to loaded plugins.. 
        // this will cause them to unload themselves
        // Unload one by one, reverse load order
        while (!_loadedPlugins.empty())
        {
            auto plugin = std::move(_loadedPlugins.back());
            _loadedPlugins.pop_back();

            const auto refs = plugin->Get().use_count();
            if (refs > 1) // We have one ref above
            {
                if (plugin == _loggingPlugin)
                {
                    continue;
                }
                TBX_ASSERT(false, "{} Plugin is still in use! Ensure all references are released before shutting down!", plugin->GetInfo().GetName());
            }

            TBX_TRACE_INFO("Unloading plugin: {}", plugin->GetInfo().GetName());
            plugin.reset(); // plugin unloads itself
        }

        // Close log and release the log plugin after we've unloaded everything else
        Log::Close(); 
        _loggingPlugin.reset();
    }

    void PluginServer::RegisterPlugin(const std::shared_ptr<LoadedPlugin>& plugin)
    {
        _loadedPlugins.push_back(plugin);
    }

    const std::vector<std::shared_ptr<LoadedPlugin>>& PluginServer::GetAll()
    {
        return _loadedPlugins;
    }

    std::vector<PluginInfo> PluginServer::SearchDirectoryForInfos(const std::string& pathToPlugins)
    {
        std::vector<PluginInfo> foundPluginInfos = {};

        const auto& filesInPluginDir = std::filesystem::directory_iterator(pathToPlugins);
        for (const auto& entry : filesInPluginDir)
        {
            // Recursively search directories
            if (entry.is_directory())
            {
                auto pluginInfosFoundInDir = SearchDirectoryForInfos(entry.path().string());
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