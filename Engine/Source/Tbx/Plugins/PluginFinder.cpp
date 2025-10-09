#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginFinder.h"
#include "Tbx/Plugins/PluginMetaReader.h"
#include "Tbx/Debug/Tracers.h"
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Tbx
{
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

        return meta;
    }

    PluginFinder::PluginFinder(
        std::string searchDirectory,
        std::vector<std::string> requestedPlugins)
        : _requested(std::move(requestedPlugins))
    {
        TBX_TRACE_INFO("PluginFinder: Discovering plugins....");
        _discovered = Discover(searchDirectory);

        if (_requested.empty())
        {
            TBX_TRACE_INFO("PluginFinder: Found {} plugin definitions", _discovered.size());
            return;
        }

        std::unordered_map<std::string, PluginMeta> discoveredByName;
        discoveredByName.reserve(_discovered.size());
        for (const auto& info : _discovered)
        {
            discoveredByName.emplace(info.Name, info);
        }

        std::vector<PluginMeta> filtered = {};
        filtered.reserve(_discovered.size());
        std::unordered_set<std::string> included = {};
        included.reserve(_discovered.size());
        std::unordered_set<std::string> visiting = {};
        visiting.reserve(_discovered.size());

        std::function<bool(const std::string&, const std::string&)> includePlugin =
            [&](const std::string& name, const std::string& parent)
            {
                if (name == "All")
                {
                    return true;
                }

                if (included.contains(name))
                {
                    return true;
                }

                auto it = discoveredByName.find(name);
                if (it == discoveredByName.end())
                {
                    if (parent.empty())
                    {
                        TBX_ASSERT(false, "PluginFinder: Requested plugin '{}' was not discovered.", name);
                    }
                    else
                    {
                        TBX_ASSERT(
                            false,
                            "PluginFinder: Dependency '{}' for plugin '{}' was not discovered.",
                            name,
                            parent);
                    }
                    return false;
                }

                if (!visiting.insert(name).second)
                {
                    TBX_ASSERT(false, "PluginFinder: Detected cyclic dependency involving '{}'.", name);
                    return false;
                }

                bool dependenciesResolved = true;
                for (const auto& dependency : it->second.Dependencies)
                {
                    dependenciesResolved &= includePlugin(dependency, it->second.Name);
                }

                visiting.erase(name);
                if (!dependenciesResolved)
                {
                    return false;
                }

                filtered.push_back(it->second);
                included.insert(name);
                return true;
            };

        for (const auto& name : _requested)
        {
            includePlugin(name, std::string());
        }

        _discovered = std::move(filtered);
        TBX_TRACE_INFO("PluginFinder: Found {} plugin definitions", _discovered.size());
    }

    std::vector<PluginMeta> PluginFinder::Discover(const std::string& searchDirectory) const
    {
        std::vector<PluginMeta> foundPluginInfos = {};

        if (!std::filesystem::exists(searchDirectory))
        {
            TBX_TRACE_ERROR("PluginFinder: Plugin search directory '{}' does not exist.", searchDirectory);
            return foundPluginInfos;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 searchDirectory,
                 std::filesystem::directory_options::skip_permission_denied))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".meta")
            {
                continue;
            }

            auto plugInfo = LoadPluginMeta(entry.path());
            const bool pluginInfoValid = !plugInfo.Name.empty();
            TBX_ASSERT(pluginInfoValid, "PluginFinder: Invalid plugin info at: {0}!", entry.path().string());
            if (!pluginInfoValid)
            {
                continue;
            }

            foundPluginInfos.push_back(std::move(plugInfo));
        }

        return foundPluginInfos;
    }

    std::vector<PluginMeta> PluginFinder::Result() &&
    {
        return std::move(_discovered);
    }
}
