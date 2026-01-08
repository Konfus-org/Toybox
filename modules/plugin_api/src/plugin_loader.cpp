#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_registry.h"
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx
{
    static std::filesystem::path resolve_library_path(
        const PluginMeta& meta,
        IFileSystem& file_ops)
    {
        std::filesystem::path library_path = meta.library_path;

        if (library_path.empty())
            library_path = meta.root_directory;

        if (file_ops.get_file_type(library_path) == FilePathType::Directory)
            library_path /= meta.name;

        if (library_path.extension().string().empty())
        {
#if defined(TBX_PLATFORM_WINDOWS)
            library_path.replace_extension(".dll");
#elif defined(TBX_PLATFORM_MACOS)
            const std::string file_name = library_path.filename().string();
            if (!file_name.starts_with("lib"))
                library_path = library_path.parent_path() / ("lib" + file_name);
            library_path.replace_extension(".dylib");
#else
            const std::string file_name = library_path.filename().string();
            if (!file_name.starts_with("lib"))
                library_path = library_path.parent_path() / ("lib" + file_name);
            library_path.replace_extension(".so");
#endif
        }

        return library_path;
    }

    static LoadedPlugin load_plugin_internal(const PluginMeta& meta, IFileSystem& file_ops)
    {
        if (meta.abi_version != PluginAbiVersion)
        {
            if (get_global_dispatcher())
            {
                TBX_TRACE_WARNING(
                    "Plugin ABI mismatch for {}: expected {}, found {}",
                    meta.name,
                    PluginAbiVersion,
                    meta.abi_version);
            }
            return {};
        }

        LoadedPlugin loaded;
        loaded.meta = meta;

        if (meta.linkage == PluginLinkage::Static)
        {
            Plugin* plug = PluginRegistry::get_instance().find_plugin(meta.name);
            if (!plug)
            {
                TBX_TRACE_WARNING("Static plugin not registered: {}", meta.name);
                return {};
            }

            PluginInstance instance(
                plug,
                [](Plugin*)
                {
                });
            loaded.instance = std::move(instance);
            return loaded;
        }

        const std::filesystem::path library_path = resolve_library_path(meta, file_ops);
        auto lib = std::make_unique<SharedLibrary>(library_path);

        const std::string create_symbol = "create_" + meta.name;
        CreatePluginFn create = lib->get_symbol<CreatePluginFn>(create_symbol.c_str());
        if (!create)
        {
            TBX_TRACE_WARNING("Entry point not found in plugin module: {}", create_symbol);
            return {};
        }

        const std::string destroy_symbol = "destroy_" + meta.name;
        DestroyPluginFn destroy = lib->get_symbol<DestroyPluginFn>(destroy_symbol.c_str());
        if (!destroy)
        {
            TBX_TRACE_WARNING("Destroy entry point not found in plugin module: {}", destroy_symbol);
            return {};
        }

        Plugin* plugin_instance = create();
        if (!plugin_instance)
        {
            TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name);
            return {};
        }

        PluginInstance instance(plugin_instance, destroy);
        loaded.instance = std::move(instance);
        loaded.library = std::move(lib);

        TBX_TRACE_INFO("Loaded plugin: {}", meta.name);

        return loaded;
    }

    static std::vector<PluginMeta> resolve_plugin_load_order(
        const std::vector<PluginMeta>& plugins)
    {
        std::unordered_map<std::string, std::size_t> by_name_lookup;
        by_name_lookup.reserve(plugins.size());
        for (std::size_t index = 0; index < plugins.size(); ++index)
        {
            by_name_lookup.emplace(ToLower(plugins[index].name), index);
        }

        std::vector<std::vector<std::size_t>> dependencies(plugins.size());
        for (std::size_t index = 0; index < plugins.size(); ++index)
        {
            std::unordered_set<std::size_t> unique;
            for (const std::string& dependency : plugins[index].dependencies)
            {
                const std::string needle = ToLower(TrimString(dependency));
                auto it = by_name_lookup.find(needle);
                if (it == by_name_lookup.end() || it->second == index)
                {
                    TBX_ASSERT(
                        false,
                        "Failed to resolve dependency '{}' for '{}'",
                        dependency,
                        plugins[index].name);
                    return {};
                }

                if (unique.insert(it->second).second)
                    dependencies[index].push_back(it->second);
            }
        }

        std::vector<std::size_t> indegree(plugins.size(), 0);
        std::vector<std::vector<std::size_t>> adjacency(plugins.size());
        for (std::size_t index = 0; index < plugins.size(); ++index)
        {
            for (std::size_t dependency : dependencies[index])
            {
                adjacency[dependency].push_back(index);
                indegree[index] += 1;
            }
        }

        std::deque<std::size_t> ready;
        for (std::size_t index = 0; index < plugins.size(); ++index)
        {
            if (indegree[index] == 0)
                ready.push_back(index);
        }

        std::vector<PluginMeta> ordered;
        ordered.reserve(plugins.size());
        while (!ready.empty())
        {
            const std::size_t current = ready.front();
            ready.pop_front();
            ordered.push_back(plugins[current]);

            for (std::size_t dependent : adjacency[current])
            {
                indegree[dependent] -= 1;
                if (indegree[dependent] == 0)
                {
                    ready.push_back(dependent);
                }
            }
        }

        if (ordered.size() != plugins.size())
        {
            TBX_ASSERT(false, "Plugin dependency cycle detected while resolving load order");
            return {};
        }

        return ordered;
    }

    std::vector<LoadedPlugin> PluginLoader::load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFileSystem& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        PluginMetaParser parser;
        if (file_ops.exists(directory))
        {
            for (const std::filesystem::path& entry : file_ops.read_directory(directory))
            {
                if (file_ops.get_file_type(entry) != FilePathType::Regular)
                    continue;

                const std::string name = entry.filename().string();
                const std::string lowered_name = ToLower(name);
                if (entry.extension() == ".meta" || lowered_name == "plugin.meta")
                {
                    std::string manifest_data;
                    if (!file_ops.read_file(entry, FileDataFormat::Utf8Text, manifest_data))
                        continue;

                    PluginMeta manifest_meta;
                    if (parser.try_parse_plugin_meta(manifest_data, entry, manifest_meta))
                        discovered.push_back(manifest_meta);
                    else
                    {
                        const std::string manifest_path = entry.string();
                        TBX_ASSERT(false, "Plugin {} is unable to be loaded!", manifest_path);
                    }
                }
            }
        }

        if (discovered.empty())
            return loaded;

        std::vector<PluginMeta> metas;
        if (requested_ids.empty())
            metas = discovered;
        else
        {
            std::unordered_map<std::string, std::size_t> by_name_lookup;
            by_name_lookup.reserve(discovered.size());
            for (std::size_t index = 0; index < discovered.size(); ++index)
            {
                const std::string lowered_name = ToLower(discovered[index].name);
                by_name_lookup.emplace(lowered_name, index);
            }

            std::unordered_set<std::size_t> selected;
            std::deque<std::size_t> pending;

            auto enqueue_index = [&](std::size_t index)
            {
                if (selected.insert(index).second)
                {
                    pending.push_back(index);
                }
            };

            auto enqueue_dependency_token = [&](const std::string& token)
            {
                const std::string trimmed = TrimString(token);
                const std::string needle = ToLower(trimmed);
                if (needle.empty())
                    return;

                auto id_it = by_name_lookup.find(needle);
                if (id_it != by_name_lookup.end())
                    enqueue_index(id_it->second);
                else
                    TBX_ASSERT(false, "Requested plugin not found: {}", trimmed);
            };

            for (const std::string& requested : requested_ids)
            {
                enqueue_dependency_token(requested);
            }

            while (!pending.empty())
            {
                std::size_t index = pending.front();
                pending.pop_front();
                metas.push_back(discovered[index]);
                for (const std::string& dependency : discovered[index].dependencies)
                {
                    enqueue_dependency_token(dependency);
                }
            }
        }

        if (metas.empty())
            return loaded;

        metas = resolve_plugin_load_order(metas);
        if (metas.empty())
            return loaded;
        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops);
            if (plug.instance)
                loaded.push_back(std::move(plug));
        }

        return loaded;
    }

    std::vector<LoadedPlugin> PluginLoader::load_plugins(
        const std::vector<PluginMeta>& metas,
        IFileSystem& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops);
            if (plug.instance)
                loaded.push_back(std::move(plug));
        }

        return loaded;
    }
}
