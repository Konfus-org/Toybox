#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/common/int.h"
#include "tbx/common/string_utils.h"
#include "tbx/debugging/logging.h"
#include "tbx/debugging/macros.h"
#include "tbx/files/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_registry.h"
#include <algorithm>
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
    struct RequiredFallbackPlugin
    {
        PluginCategory category = PluginCategory::Default;
        PluginMeta meta;
    };

    static bool path_contains_directory_token(
        const std::filesystem::path& path,
        std::string_view directory_name_lowered)
    {
        if (directory_name_lowered.empty())
            return false;

        for (const auto& part : path)
        {
            if (to_lower(part.string()) == directory_name_lowered)
                return true;
        }

        return false;
    }

    static bool is_plugin_manifest_file(const std::filesystem::path& path)
    {
        const std::string lowered_name = to_lower(path.filename().string());
#if defined(TBX_PLATFORM_WINDOWS)
        return lowered_name.ends_with(".dll.meta");
#elif defined(TBX_PLATFORM_MACOS)
        return lowered_name.ends_with(".dylib.meta");
#else
        return lowered_name.ends_with(".so.meta");
#endif
    }

    static std::filesystem::path append_debug_postfix(const std::filesystem::path& library_path)
    {
        const std::string extension = library_path.extension().string();
        if (extension.empty())
            return {};

        const std::string stem = library_path.stem().string();
        if (stem.ends_with("d"))
            return {};

        const std::string debug_name = stem + "d" + extension;
        return library_path.parent_path() / debug_name;
    }

    static std::filesystem::path resolve_library_path(const PluginMeta& meta, IFileSystem& file_ops)
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

        if (!file_ops.exists(library_path))
        {
            const std::filesystem::path debug_candidate = append_debug_postfix(library_path);
            if (!debug_candidate.empty() && file_ops.exists(debug_candidate))
                return debug_candidate;
        }

        return library_path;
    }

    static LoadedPlugin load_plugin_internal(
        const PluginMeta& meta,
        IFileSystem& file_ops,
        IPluginHost& host)
    {
        if (meta.abi_version != PluginAbiVersion)
        {
            TBX_TRACE_WARNING(
                "Plugin ABI mismatch for {}: expected {}, found {}",
                meta.name,
                PluginAbiVersion,
                meta.abi_version);
            return {};
        }

        if (meta.linkage == PluginLinkage::Static)
        {
            Plugin* plug = PluginRegistry::get_instance().find_plugin(meta.name);
            if (!plug)
            {
                TBX_TRACE_WARNING("Static plugin not registered: {}", meta.name);
                return {};
            }

            auto instance = std::unique_ptr<Plugin, PluginDeleter>(
                plug,
                [](Plugin*)
                {
                });
            return LoadedPlugin(meta, nullptr, std::move(instance), host);
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

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(plugin_instance, destroy);
        return LoadedPlugin(meta, std::move(lib), std::move(instance), host);
    }

    static bool has_loaded_category(
        const std::vector<LoadedPlugin>& loaded_plugins,
        PluginCategory category)
    {
        for (const auto& plugin : loaded_plugins)
        {
            if (plugin.meta.category == category)
                return true;
        }

        return false;
    }

    static const std::vector<RequiredFallbackPlugin>& get_required_fallback_plugins()
    {
        static const std::vector<RequiredFallbackPlugin> fallbacks = []()
        {
            std::vector<RequiredFallbackPlugin> entries;

            PluginMeta logging_meta;
            logging_meta.name = std::string(get_stdout_fallback_logger_name());
            logging_meta.version = std::string(get_stdout_fallback_logger_version());
            logging_meta.description = std::string(get_stdout_fallback_logger_description());
            logging_meta.linkage = PluginLinkage::Static;
            logging_meta.category = PluginCategory::Logging;
            logging_meta.priority = 0;

            RequiredFallbackPlugin logging_fallback;
            logging_fallback.category = PluginCategory::Logging;
            logging_fallback.meta = std::move(logging_meta);
            entries.push_back(std::move(logging_fallback));

            return entries;
        }();

        return fallbacks;
    }

    static void load_required_fallback_plugins(
        std::vector<LoadedPlugin>& loaded_plugins,
        IFileSystem& file_ops,
        IPluginHost& host)
    {
        for (const RequiredFallbackPlugin& fallback : get_required_fallback_plugins())
        {
            if (has_loaded_category(loaded_plugins, fallback.category))
                continue;

            LoadedPlugin fallback_loaded = load_plugin_internal(fallback.meta, file_ops, host);
            if (fallback_loaded.is_valid())
                loaded_plugins.push_back(std::move(fallback_loaded));
        }
    }

    static std::vector<PluginMeta> resolve_plugin_load_order(const std::vector<PluginMeta>& plugins)
    {
        const auto is_before_update_order = [](const PluginMeta& left, const PluginMeta& right)
        {
            if (left.category != right.category)
            {
                return static_cast<uint32>(left.category) < static_cast<uint32>(right.category);
            }

            if (left.priority != right.priority)
                return left.priority < right.priority;

            return to_lower(left.name) < to_lower(right.name);
        };

        std::unordered_map<std::string, uint64> by_name_lookup;
        by_name_lookup.reserve(plugins.size());
        const auto plugin_count = static_cast<uint64>(plugins.size());
        for (uint64 index = 0; index < plugin_count; ++index)
        {
            by_name_lookup.emplace(to_lower(plugins[index].name), index);
        }

        std::vector<std::vector<uint64>> dependencies(plugins.size());
        for (uint64 index = 0; index < plugin_count; ++index)
        {
            std::unordered_set<uint64> unique;
            for (const std::string& dependency : plugins[index].dependencies)
            {
                const std::string needle = to_lower(trim(dependency));
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

        std::vector<uint64> indegree(plugins.size(), 0);
        std::vector<std::vector<uint64>> adjacency(plugins.size());
        for (uint64 index = 0; index < plugin_count; ++index)
        {
            for (uint64 dependency : dependencies[index])
            {
                adjacency[dependency].push_back(index);
                indegree[index] += 1;
            }
        }

        std::vector<uint64> ready;
        ready.reserve(plugin_count);
        for (uint64 index = 0; index < plugin_count; ++index)
        {
            if (indegree[index] == 0)
                ready.push_back(index);
        }

        std::vector<PluginMeta> ordered;
        ordered.reserve(plugins.size());
        while (!ready.empty())
        {
            std::sort(
                ready.begin(),
                ready.end(),
                [&](uint64 left, uint64 right)
                {
                    return is_before_update_order(plugins[left], plugins[right]);
                });
            const uint64 current = ready.front();
            ready.erase(ready.begin());
            ordered.push_back(plugins[current]);

            for (uint64 dependent : adjacency[current])
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

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFileSystem& file_ops,
        IPluginHost& host)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        PluginMetaParser parser;
        if (file_ops.exists(directory))
        {
            for (const std::filesystem::path& entry : file_ops.read_directory(directory))
            {
                // Release builds often bundle assets into a `resources/` folder near the executable.
                // Keep plugin discovery from crawling that subtree.
                if (path_contains_directory_token(entry, "resources"))
                    continue;

                if (file_ops.get_file_type(entry) != FilePathType::Regular)
                    continue;

                // Only treat manifests as plugin metadata when their filename also contains the platform's
                // dynamic library extension (e.g. `Foo.dll.meta`, `libFoo.so.meta`).
                if (!is_plugin_manifest_file(entry))
                    continue;

                std::string manifest_data;
                if (!file_ops.read_file(entry, FileDataFormat::Utf8Text, manifest_data))
                    continue;

                PluginMeta manifest_meta;
                if (parser.try_parse_plugin_meta(manifest_data, entry, manifest_meta))
                    discovered.push_back(manifest_meta);
                else
                {
                    const std::string manifest_path = entry.string();
                    TBX_TRACE_WARNING("Plugin {} is unable to be loaded!", manifest_path);
                }
            }
        }

        if (discovered.empty())
        {
            load_required_fallback_plugins(loaded, file_ops, host);
            return loaded;
        }

        std::vector<PluginMeta> metas;
        if (requested_ids.empty())
            metas = discovered;
        else
        {
            std::unordered_map<std::string, uint64> by_name_lookup;
            by_name_lookup.reserve(discovered.size());
            const auto discovered_count = static_cast<uint64>(discovered.size());
            for (uint64 index = 0; index < discovered_count; ++index)
            {
                const std::string lowered_name = to_lower(discovered[index].name);
                by_name_lookup.emplace(lowered_name, index);
            }

            std::unordered_set<uint64> selected;
            std::deque<uint64> pending;

            auto enqueue_index = [&](uint64 index)
            {
                if (selected.insert(index).second)
                {
                    pending.push_back(index);
                }
            };

            auto enqueue_dependency_token = [&](const std::string& token)
            {
                const std::string trimmed = trim(token);
                const std::string needle = to_lower(trimmed);
                if (needle.empty())
                    return;

                auto id_it = by_name_lookup.find(needle);
                if (id_it != by_name_lookup.end())
                    enqueue_index(id_it->second);
                else
                    TBX_TRACE_WARNING("Requested plugin not found: {}", trimmed);
            };

            for (const std::string& requested : requested_ids)
            {
                enqueue_dependency_token(requested);
            }

            while (!pending.empty())
            {
                uint64 index = pending.front();
                pending.pop_front();
                metas.push_back(discovered[index]);
                for (const std::string& dependency : discovered[index].dependencies)
                {
                    enqueue_dependency_token(dependency);
                }
            }
        }

        if (metas.empty())
        {
            load_required_fallback_plugins(loaded, file_ops, host);
            return loaded;
        }

        metas = resolve_plugin_load_order(metas);
        if (metas.empty())
        {
            load_required_fallback_plugins(loaded, file_ops, host);
            return loaded;
        }
        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops, host);
            if (plug.is_valid())
                loaded.push_back(std::move(plug));
            else
                TBX_TRACE_WARNING("Failed to load plugin: {}", meta.name);
        }

        load_required_fallback_plugins(loaded, file_ops, host);
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        IFileSystem& file_ops,
        IPluginHost& host)
    {
        std::vector<LoadedPlugin> loaded;

        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops, host);
            if (plug.is_valid())
                loaded.push_back(std::move(plug));
        }

        load_required_fallback_plugins(loaded, file_ops, host);
        return loaded;
    }
}
