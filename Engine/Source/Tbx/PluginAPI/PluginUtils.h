#pragma once
#include "Tbx/PCH.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Events/EventCoordinator.h"
#include <filesystem>
#include <algorithm>
#include <unordered_set>
#include <sstream>

namespace Tbx::Plugin
{
    /// <summary>
    /// Returns true if the plugin implements the given type.
    /// </summary>
    static bool ImplementsType(const PluginInfo& info, std::string_view type)
    {
        const auto& types = info.GetImplementedTypes();
        return std::find(types.begin(), types.end(), type) != types.end();
    }

    /// <summary>
    /// Dependencies satisfied if each dep is either:
    ///  - the name of a loaded plugin, OR
    ///  - a type that at least one loaded plugin implements.
    /// </summary>
    static bool AreDependenciesSatisfied(
        const PluginInfo& info,
        const std::unordered_set<std::string>& loadedNames,
        const std::unordered_set<std::string>& loadedTypes)
    {
        for (const auto& dep : info.GetDependencies())
        {
            if (dep == "All")
            {
                continue;
            }
            if (loadedNames.count(dep) || loadedTypes.count(dep))
            {
                continue;
            }
            return false;
        }
        return true;
    }

    /// <summary>
    /// Load one plugin, update name/type sets, emit event.
    /// </summary>
    static bool Load(
        const PluginInfo& info,
        std::vector<std::shared_ptr<LoadedPlugin>>& outLoaded,
        std::unordered_set<std::string>& loadedNames,
        std::unordered_set<std::string>& loadedTypes)
    {
        auto plugin = std::make_shared<LoadedPlugin>(info);
        if (!plugin->IsValid())
        {
            TBX_ASSERT(false, "Failed to load plugin: {0}", info.GetName());
            return false;
        }

        outLoaded.push_back(plugin);
        loadedNames.insert(plugin->GetInfo().GetName());
        for (const auto& t : plugin->GetInfo().GetImplementedTypes())
        {
            loadedTypes.insert(t);
        }

        auto e = PluginLoadedEvent(plugin);
        EventCoordinator::Send(e);

        return true;
    }

    /// <summary>Sort key: fewer dependencies first, then by name for stability.</summary>
    static bool SortKey(const PluginInfo& a, const PluginInfo& b)
    {
        const auto da = a.GetDependencies().size();
        const auto db = b.GetDependencies().size();
        if (da != db) return da < db;
        return a.GetName() < b.GetName();
    }

    /// <summary>
    /// Helpful error text when dependency resolution stalls.
    /// </summary>
    static void ReportUnresolvedDependencies(
        const std::vector<PluginInfo>& remaining,
        const std::unordered_set<std::string>& loadedNames,
        const std::unordered_set<std::string>& loadedTypes)
    {
        std::ostringstream oss;
        oss << "Unresolved plugin dependencies:\n";
        for (const auto& pi : remaining)
        {
            std::vector<std::string> missing;
            for (const auto& dep : pi.GetDependencies())
            {
                if (!loadedNames.count(dep) && !loadedTypes.count(dep))
                {
                    missing.push_back(dep);
                }
            }
            oss << "  - " << pi.GetName();
            if (!missing.empty())
            {
                oss << " (missing: ";
                for (size_t i = 0; i < missing.size(); ++i) {
                    if (i) oss << ", ";
                    {
                        oss << missing[i];
                    }
                }
                oss << ")";
            }
            oss << "\n";
        }
        TBX_TRACE_ERROR("{}", oss.str());
    }

    /// <summary>
    /// Reports info on loaded plugin.
    /// </summary>
    static void ReportInfo(const PluginInfo& info)
    {
        const auto& pluginName = info.GetName();
        const auto& pluginVersion = info.GetVersion();
        const auto& pluginAuthor = info.GetAuthor();
        const auto& pluginDescription = info.GetDescription();

        TBX_TRACE_INFO("{0}:", pluginName);
        TBX_TRACE_INFO("    - Version: {0}", pluginVersion);
        TBX_TRACE_INFO("    - Author: {0}", pluginAuthor);
        TBX_TRACE_INFO("    - Description: {0}", pluginDescription);
    }
}