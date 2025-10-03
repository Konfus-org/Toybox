#include "Tbx/PCH.h"
#include "Tbx/Plugins/PluginFinder.h"
#include "Tbx/Plugins/PluginMetaReader.h"
#include "Tbx/Debug/Debugging.h"
#include <filesystem>
#include <unordered_set>
#include <utility>

namespace Tbx
{
    namespace PluginFinderDetail
    {
        PluginMeta LoadPluginMeta(const std::filesystem::path& pathToMeta)
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

        std::unordered_set<std::string> requestedNames(_requested.begin(), _requested.end());
        std::vector<PluginMeta> filtered;
        filtered.reserve(_requested.size());

        for (const auto& info : _discovered)
        {
            if (requestedNames.contains(info.Name))
            {
                filtered.push_back(info);
                requestedNames.erase(info.Name);
            }
        }

        for (const auto& missing : requestedNames)
        {
            TBX_TRACE_ERROR("PluginFinder: Requested plugin '{}' was not discovered.", missing);
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

            auto plugInfo = PluginFinderDetail::LoadPluginMeta(entry.path());
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
