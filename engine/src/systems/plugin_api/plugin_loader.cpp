#include "tbx/systems/plugin_api/plugin_loader.h"
#include "systems/plugin_api/internal/plugin_loader_internal.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <numeric>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
namespace tbx
{
    bool is_plugin_library_path(const std::filesystem::path& path)
    {
        const std::string lowered_name = to_lower(path.filename().string());
#if defined(TBX_PLATFORM_WINDOWS)
        return lowered_name.ends_with(".dll");
#elif defined(TBX_PLATFORM_MACOS)
        return lowered_name.ends_with(".dylib");
#else
        return lowered_name.ends_with(".so");
#endif
    }

    std::filesystem::path resolve_plugin_library_path(const PluginMeta& meta, IFileOps& file_ops)
    {
        std::filesystem::path library_path = meta.library_path;

        if (library_path.empty())
            library_path = meta.root_directory;

        if (file_ops.get_type(library_path) == FileType::DIRECTORY)
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
            const std::filesystem::path debug_candidate =
                internal::append_debug_postfix(library_path);
            if (!debug_candidate.empty() && file_ops.exists(debug_candidate))
                return debug_candidate;
        }

        return library_path;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFileOps& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        if (file_ops.exists(directory))
        {
            for (const std::filesystem::path& entry : file_ops.read_directory(directory))
            {
                // Keep plugin discovery from crawling bundled assets or loader-owned shadow
                // copies near the executable.
                if (internal::path_contains_directory_token(entry, "resources"))
                    continue;
                if (internal::path_contains_directory_token(
                        entry,
                        internal::PluginShadowCopyDirectory))
                    continue;

                if (file_ops.get_type(entry) != FileType::FILE)
                    continue;

                if (!is_plugin_library_path(entry))
                    continue;

                auto meta = PluginMeta {};
                if (internal::try_query_plugin_meta_from_library(entry, file_ops, meta))
                    discovered.push_back(std::move(meta));
            }
        }

        if (discovered.empty())
        {
            TBX_TRACE_WARNING("No plugins found at {}", directory.string());
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
            return loaded;

        metas = internal::resolve_plugin_load_order(metas);
        if (metas.empty())
            return loaded;
        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = internal::load_plugin_internal(meta, file_ops);
            if (plug.is_valid())
                loaded.push_back(std::move(plug));
            else
                TBX_TRACE_WARNING("Failed to load plugin: {}", meta.name);
        }

        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        const std::filesystem::path& working_directory)
    {
        auto file_ops = FileOperator(working_directory);
        return load_plugins(directory, requested_ids, file_ops);
    }

    std::vector<LoadedPlugin> load_plugins(const std::vector<PluginMeta>& metas, IFileOps& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = internal::load_plugin_internal(meta, file_ops);
            if (plug.is_valid())
                loaded.push_back(std::move(plug));
        }

        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        const std::filesystem::path& working_directory)
    {
        auto file_ops = FileOperator(working_directory);
        return load_plugins(metas, file_ops);
    }

    void update_plugins(std::vector<LoadedPlugin>& loaded_plugins, const DeltaTime& dt)
    {
        auto ordered_indices = internal::build_update_order(loaded_plugins, false);
        for (size index : ordered_indices)
        {
            auto& plugin = loaded_plugins[index];
            if (!plugin.instance)
                continue;

            plugin.instance->update(dt);
        }
    }

    void update_plugins_fixed(std::vector<LoadedPlugin>& loaded_plugins, const DeltaTime& dt)
    {
        auto ordered_indices = internal::build_update_order(loaded_plugins, true);
        for (size index : ordered_indices)
        {
            auto& plugin = loaded_plugins[index];
            if (!plugin.instance)
                continue;

            plugin.instance->fixed_update(dt);
        }
    }

    void detach_plugins(
        std::vector<LoadedPlugin>& loaded_plugins,
        ServiceProvider& service_provider,
        IMessageCoordinator* coordinator)
    {
        const size plugin_count = static_cast<size>(loaded_plugins.size());
        auto detached = std::vector<bool>(plugin_count, false);
        size detached_count = 0U;

        while (detached_count < plugin_count)
        {
            auto name_to_index = std::unordered_map<std::string, size> {};
            name_to_index.reserve(plugin_count - detached_count);
            for (size index = 0; index < plugin_count; ++index)
            {
                if (!detached[index])
                    name_to_index.emplace(to_lower(loaded_plugins[index].meta.name), index);
            }

            auto dependents_count = std::vector<size>(plugin_count, size {0});
            for (size index = 0; index < plugin_count; ++index)
            {
                if (detached[index])
                    continue;

                for (const std::string& dependency : loaded_plugins[index].meta.dependencies)
                {
                    const std::string lowered = to_lower(trim(dependency));
                    auto dependency_it = name_to_index.find(lowered);
                    if (dependency_it == name_to_index.end())
                        continue;

                    dependents_count[dependency_it->second] += 1U;
                }
            }

            auto candidates = std::vector<size> {};
            candidates.reserve(plugin_count - detached_count);
            for (size index = 0; index < plugin_count; ++index)
            {
                if (!detached[index] && dependents_count[index] == 0U)
                    candidates.push_back(index);
            }

            if (candidates.empty())
            {
                TBX_TRACE_WARNING(
                    "Plugin unload dependency cycle detected. Falling back to stack order.");
                for (size index = plugin_count; index > 0U; --index)
                {
                    const size selected_index = index - 1U;
                    if (detached[selected_index])
                        continue;

                    candidates.push_back(selected_index);
                    break;
                }
            }

            std::sort(
                candidates.begin(),
                candidates.end(),
                [&loaded_plugins](size left_index, size right_index)
                {
                    const PluginMeta& left = loaded_plugins[left_index].meta;
                    const PluginMeta& right = loaded_plugins[right_index].meta;
                    return internal::should_unload_before(left, right);
                });

            const size selected_index = candidates.front();
            loaded_plugins[selected_index].detach(service_provider);
            if (coordinator)
                coordinator->flush();

            detached[selected_index] = true;
            ++detached_count;
        }
    }

    void unload_plugins(
        std::vector<LoadedPlugin>& loaded_plugins,
        ServiceProvider& service_provider,
        IMessageCoordinator* coordinator)
    {
        detach_plugins(loaded_plugins, service_provider, coordinator);
        loaded_plugins.clear();
    }
}
