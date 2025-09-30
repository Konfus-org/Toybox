#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginServer.h"
#include "Tbx/Plugins/PluginMetaReader.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Debug/Debugging.h"
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace Tbx
{
    //////////// UTILS //////////////////

    /// <summary>
    /// Dependencies satisfied if each dep is either:
    ///  - the name of a loaded plugin, OR
    ///  - a type that at least one loaded plugin implements.
    /// </summary>
    static bool ArePluginDependenciesSatisfied(
        const PluginMeta& info,
        const std::unordered_set<std::string>& loadedNames)
    {
        for (const auto& dep : info.Dependencies)
        {
            if (dep == "All")
            {
                continue;
            }
            if (loadedNames.contains(dep))
            {
                continue;
            }
            return false;
        }
        return true;
    }

    /// <summary>
    /// Helpful error text when dependency resolution stalls.
    /// </summary>
    static void ReportUnresolvedPluginDependencies(
        const std::vector<PluginMeta>& remaining,
        const std::unordered_set<std::string>& loadedNames)
    {
        std::ostringstream oss = {};
        oss << "Unresolved plugin dependencies:\n";
        for (const auto& pi : remaining)
        {
            std::vector<std::string> missing;
            for (const auto& dep : pi.Dependencies)
            {
                if (!loadedNames.contains(dep))
                {
                    missing.push_back(dep);
                }
            }
            oss << "  - " << pi.Name;
            if (!missing.empty())
            {
                oss << " (missing: ";
                for (size_t i = 0; i < missing.size(); ++i)
                {
                    if (i) oss << ", ";
                    {
                        oss << missing[i];
                    }
                }
                oss << ")";
            }
            oss << "\n";
        }
        TBX_TRACE_ERROR("PluginServer: {}", oss.str());
    }

    /// <summary>
    /// Reports info on loaded plugin.
    /// </summary>
    static void ReportPluginInfo(const PluginMeta& info)
    {
        const auto& pluginName = info.Name;
        const auto& pluginVersion = info.Version;
        const auto& pluginAuthor = info.Author;
        const auto& pluginDescription = info.Description;

        TBX_TRACE_INFO("- Loaded {0}:", pluginName);
        TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
        TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
        TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
    }

    static PluginMeta LoadPluginMeta(const std::filesystem::path& pathToMeta)
    {
        auto metaData = PluginMetaReader::Read(pathToMeta.string());
        if (metaData.empty()) return {};

        auto meta = PluginMeta();
        meta.Name = metaData["name"][0];
#ifdef TBX_PLATFORM_WINDOWS
        meta.Path = pathToMeta.parent_path().string() + "/" + pathToMeta.filename().stem().string() + ".dll";
#else
        meta.Path = pathToMeta.parent_path().string() + "/" + pathToMeta.filename().stem().string() + ".so";
#endif
        meta.Author = metaData["author"][0];
        meta.Version = metaData["version"][0];
        meta.Description = metaData["description"][0];
        meta.Dependencies = metaData["dependencies"];
        meta.IsStatic = false;

        return meta;
    }

    /// <summary>
    /// Load one plugin, update name/type sets, emit event.
    /// </summary>
    static bool LoadPlugin(
        const PluginMeta& info,
        Ref<EventBus> eventBus,
        std::unordered_set<std::string>& outLoadedPluginNames,
        std::vector<ExclusiveRef<PluginServerRecord>>& outLoaded)
    {
        auto loadedPlugin = MakeExclusive<PluginServerRecord>(info, eventBus);
        if (!loadedPlugin || !loadedPlugin->IsValid())
        {
            TBX_ASSERT(false, "PluginServer: Failed to load plugin: {0}", info.Name);
            return false;
        }

        auto plugin = loadedPlugin->Get();
        eventBus->Post(PluginLoadedEvent(plugin));
        ReportPluginInfo(info);

        outLoadedPluginNames.insert(loadedPlugin->GetMeta().Name);
        outLoaded.push_back(std::move(loadedPlugin));

        return true;
    }

    /// <summary>
    /// Sort key: fewer dependencies first, then by name for stability.
    /// </summary>
    static bool SortKey(const PluginMeta& a, const PluginMeta& b)
    {
        const auto da = a.Dependencies.size();
        const auto db = b.Dependencies.size();
        if (da != db) return da < db;
        return a.Name < b.Name;
    }

    //////////// PLUGIN MANAGER //////////////////

    PluginServer::PluginServer(
        const std::string& pathToPlugins,
        Ref<EventBus> eventBus)
    {
        _eventBus = eventBus;
        LoadPlugins(pathToPlugins);
    }

    PluginServer::~PluginServer()
    {
        UnloadPlugins();
    }

    void PluginServer::RegisterPlugin(ExclusiveRef<PluginServerRecord> plugin)
    {
        _pluginRecords.push_back(std::move(plugin));
    }

    PluginStack PluginServer::GetPlugins() const
    {
        PluginStack result = {};
        for (const auto& owned : _pluginRecords)
        {
            result.Push(owned->Get());
        }
        return result;
    }

    std::vector<PluginMeta> PluginServer::SearchDirectoryForInfos(const std::string& pathToPlugins)
    {
        std::vector<PluginMeta> foundPluginInfos = {};

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
            if (entry.path().extension() == ".meta")
            {
                auto plugInfo = LoadPluginMeta(entry.path());
                auto pluginInfoValid = !plugInfo.Name.empty();
                TBX_ASSERT(pluginInfoValid, "PluginServer: Invalid plugin info at: {0}!", entry.path().string());
                if (!pluginInfoValid) continue;

                foundPluginInfos.push_back(plugInfo);
            }
        }

        return foundPluginInfos;
    }

    void PluginServer::LoadPlugins(const std::string& pathToPlugins)
    {
        // 1) Discover
        TBX_TRACE_INFO("PluginServer: Discovering plugins....");
        auto infos = SearchDirectoryForInfos(pathToPlugins);

        // 2.) Sort
        std::stable_sort(infos.begin(), infos.end(), SortKey);

        // 3) Load 
        TBX_TRACE_INFO("PluginServer: Loading plugins:");
        auto loadedNames = std::unordered_set<std::string>();
        uint pluginsSuccessfullyLoaded = 0;
        uint pluginsUnsuccessfullyLoaded = 0;
        while (!infos.empty())
        {
            bool resolvedDeps = false;

            // Iterate and grab everything that’s ready this round
            for (auto it = infos.begin(); it != infos.end(); )
            {
                if (ArePluginDependenciesSatisfied(*it, loadedNames))
                {
                    LoadPlugin(*it, _eventBus, loadedNames, _pluginRecords);
                    it = infos.erase(it);
                    pluginsSuccessfullyLoaded++;
                    resolvedDeps = true;
                }
                else
                {
                    ++it;
                    pluginsUnsuccessfullyLoaded++;
                }
            }

            if (!resolvedDeps)
            {
                TBX_ASSERT(false, "PluginServer: Unable to resolve some plugin dependencies!");
                ReportUnresolvedPluginDependencies(infos, loadedNames);
                break;
            }
        }

        TBX_TRACE_INFO("PluginServer: Successfully loaded {} plugins!", pluginsSuccessfullyLoaded);
        TBX_TRACE_INFO("PluginServer: Failed to load {} plugins!\n", pluginsUnsuccessfullyLoaded);

        _eventBus->Post(PluginsLoadedEvent(GetPlugins()));
    }

    void PluginServer::UnloadPlugins()
    {
        TBX_TRACE_INFO("PluginServer: Unloading plugins...");

        // Ensure ILogger implementations are unloaded after all other plugins.
        std::stable_partition(
            _pluginRecords.begin(),
            _pluginRecords.end(),
            [](const ExclusiveRef<PluginServerRecord>& record)
            {
                return record->GetAs<ILogger>() != nullptr;
            });

        // Clear remaining logger plugs
        while (!_pluginRecords.empty())
        {
            RemoveBackPlugin(_pluginRecords);
        }

        _eventBus->Send(PluginsUnloadedEvent(GetPlugins()));
    }

    void PluginServer::RemoveBackPlugin(std::vector<Tbx::ExclusiveRef<Tbx::PluginServerRecord>>& plugs)
    {
        auto& plug = plugs.back();
        TBX_TRACE_INFO("PluginServer: Unloading {}", plug->GetMeta().Name);
        auto pluginRecord = std::move(plug);

        auto plugin = pluginRecord->Get();
        _eventBus->Send(PluginUnloadedEvent(plugin));
        plugs.pop_back();

        // We have two refs above, one from the get call and one from the record.
        // If there are more than two refs, something outside the plugin server is still holding a ref and shouldn't be...
        // This asserts because it can cause crashes or undefined behavior if a plugin is unloaded while still in use and we are reloading.
        auto expectedUseCount = 2;
        if (pluginRecord->GetAs<ILogger>())
        {
            expectedUseCount = 3; // Logger plugins have one extra ref from the Log system
        }
        if (plugin.use_count() > expectedUseCount)
        {
            TBX_ASSERT(false, "{} Plugin is still in use! Ensure all references are released before shutting down!", pluginRecord->GetMeta().Name);
        }
    }
}