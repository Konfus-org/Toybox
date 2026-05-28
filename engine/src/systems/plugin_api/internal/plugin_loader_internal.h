#pragma once
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tbx::internal
{
    inline constexpr std::string_view PluginShadowCopyDirectory = ".plugin_load_copies";

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

    static std::filesystem::path make_plugin_shadow_copy_path(
        const std::filesystem::path& library_path,
        IFileOps& file_ops)
    {
        static uint64 next_shadow_copy_index = 0U;

        const auto copy_stem = library_path.stem().string();
        const auto copy_root = library_path.parent_path() / PluginShadowCopyDirectory;

        for (auto attempt = 0U; attempt < 128U; ++attempt)
        {
            const auto copy_directory =
                copy_root / (copy_stem + ".load_copy_" + std::to_string(++next_shadow_copy_index));
            const auto copy_path = copy_directory / library_path.filename();
            if (!file_ops.exists(copy_path))
                return copy_path;
        }

        return {};
    }

    static bool filesystem_path_exists(const std::filesystem::path& path)
    {
        auto error = std::error_code {};
        const bool exists = std::filesystem::exists(path, error);
        return exists && !error;
    }

    static std::filesystem::path find_plugin_pdb_path(const std::filesystem::path& library_path)
    {
        const auto pdb_filename = library_path.stem().string() + ".pdb";
        const auto library_directory = library_path.parent_path();

        const auto pdb_next_to_library = library_directory / pdb_filename;
        if (filesystem_path_exists(pdb_next_to_library))
            return pdb_next_to_library;

        if (library_directory.has_parent_path())
        {
            const auto config_directory = library_directory.filename();
            const auto output_directory = library_directory.parent_path();
            if (output_directory.has_parent_path())
            {
                const auto pdb_output_path =
                    output_directory.parent_path() / "pdb" / config_directory / pdb_filename;
                if (filesystem_path_exists(pdb_output_path))
                    return pdb_output_path;
            }

            const auto single_config_pdb_path =
                output_directory.parent_path() / "pdb" / pdb_filename;
            if (filesystem_path_exists(single_config_pdb_path))
                return single_config_pdb_path;
        }

        return {};
    }

    static void try_copy_plugin_pdb_for_shadow_copy(
        const std::filesystem::path& library_path,
        const std::filesystem::path& shadow_copy_path)
    {
        const auto pdb_path = find_plugin_pdb_path(library_path);
        if (pdb_path.empty())
            return;

        const auto shadow_pdb_path = shadow_copy_path.parent_path() / pdb_path.filename();
        auto copy_error = std::error_code {};
        std::filesystem::copy_file(
            pdb_path,
            shadow_pdb_path,
            std::filesystem::copy_options::overwrite_existing,
            copy_error);
        if (copy_error)
        {
            TBX_TRACE_WARNING(
                "Failed to copy plugin PDB '{}' for shadow copy '{}': {}.",
                pdb_path.string(),
                shadow_copy_path.string(),
                copy_error.message());
        }
    }

    static std::filesystem::path try_create_plugin_shadow_copy(
        const std::filesystem::path& library_path,
        IFileOps& file_ops)
    {
        if (file_ops.get_type(library_path) != FileType::FILE)
            return {};

        if (path_contains_directory_token(library_path, PluginShadowCopyDirectory))
            return {};

        const auto shadow_copy_path = make_plugin_shadow_copy_path(library_path, file_ops);
        if (shadow_copy_path.empty())
            return {};

        const auto resolved_library_path = file_ops.resolve(library_path);
        const auto resolved_shadow_copy_path = file_ops.resolve(shadow_copy_path);
        auto copy_error = std::error_code {};
        constexpr uint max_copy_attempts = 20U;
        constexpr auto copy_retry_delay = std::chrono::milliseconds(50);
        for (uint attempt = 0U; attempt < max_copy_attempts; ++attempt)
        {
            copy_error.clear();
            std::filesystem::create_directories(
                resolved_shadow_copy_path.parent_path(),
                copy_error);
            if (copy_error)
                break;

            std::filesystem::copy_file(
                resolved_library_path,
                resolved_shadow_copy_path,
                std::filesystem::copy_options::overwrite_existing,
                copy_error);
            if (!copy_error)
            {
                try_copy_plugin_pdb_for_shadow_copy(
                    resolved_library_path,
                    resolved_shadow_copy_path);
                return shadow_copy_path;
            }

            std::this_thread::sleep_for(copy_retry_delay);
        }

        TBX_TRACE_WARNING(
            "Failed to create plugin shadow copy '{}' from '{}': {}. Falling back to the "
            "original module path.",
            resolved_shadow_copy_path.string(),
            resolved_library_path.string(),
            copy_error.message());
        return {};
    }

    static std::unique_ptr<SharedLibrary> load_plugin_library(
        const std::filesystem::path& library_path,
        IFileOps& file_ops)
    {
        auto load_path = library_path;
        auto cleanup_path = std::filesystem::path {};

#if !defined(TBX_FULL_RELEASE)
        if (const auto shadow_copy_path = try_create_plugin_shadow_copy(library_path, file_ops);
            !shadow_copy_path.empty())
        {
            load_path = shadow_copy_path;
            cleanup_path = shadow_copy_path.parent_path();
        }
#endif

        auto lib = std::make_unique<SharedLibrary>(load_path, cleanup_path);
        if (!lib->is_valid() && load_path != library_path)
        {
            auto shadow_copy_error_message = std::string {};
            const bool has_shadow_copy_error_message =
                lib->try_get_load_error_message(shadow_copy_error_message);

            if (has_shadow_copy_error_message)
            {
                TBX_TRACE_WARNING(
                    "Failed to load plugin shadow copy '{}': {}. Retrying with original module "
                    "'{}'.",
                    load_path.string(),
                    shadow_copy_error_message,
                    library_path.string());
            }
            else
            {
                TBX_TRACE_WARNING(
                    "Failed to load plugin shadow copy '{}'. Retrying with original module '{}'.",
                    load_path.string(),
                    library_path.string());
            }

            load_path = library_path;
            cleanup_path.clear();
            lib = std::make_unique<SharedLibrary>(load_path, cleanup_path);
        }

        if (!lib->is_valid())
        {
            auto load_error_message = std::string {};
            if (lib->try_get_load_error_message(load_error_message))
            {
                TBX_TRACE_WARNING(
                    "Failed to load plugin module '{}': {}",
                    load_path.string(),
                    load_error_message);
            }
            else
            {
                TBX_TRACE_WARNING("Failed to load plugin module '{}'.", load_path.string());
            }
            return nullptr;
        }

        return lib;
    }

    static bool try_query_plugin_meta_from_library(
        const std::filesystem::path& library_path,
        IFileOps& file_ops,
        PluginMeta& out_meta)
    {
        auto lib = load_plugin_library(library_path, file_ops);
        if (!lib || !lib->is_valid())
            return false;

        GetPluginMetaFn get_meta = lib->get_symbol<GetPluginMetaFn>("tbx_get_plugin_meta");
        if (!get_meta)
            return false;

        auto meta = PluginMeta {};
        get_meta(&meta);
        if (meta.name.empty() || meta.version.empty())
            return false;

        meta.root_directory = library_path.parent_path();
        meta.library_path = library_path;
        meta.linkage = PluginLinkage::DYNAMIC;
        out_meta = std::move(meta);
        return true;
    }

    static LoadedPlugin load_plugin_internal(const PluginMeta& meta, IFileOps& file_ops)
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

        const std::filesystem::path library_path = resolve_plugin_library_path(meta, file_ops);
        auto lib = load_plugin_library(library_path, file_ops);
        if (!lib || !lib->is_valid())
            return {};

        CreatePluginFn create = lib->get_symbol<CreatePluginFn>("tbx_create_plugin");
        if (!create)
        {
            TBX_TRACE_WARNING(
                "Entry point not found in plugin module '{}': {}",
                lib->get_path().string(),
                "tbx_create_plugin");
            return {};
        }

        DestroyPluginFn destroy = lib->get_symbol<DestroyPluginFn>("tbx_destroy_plugin");
        if (!destroy)
        {
            TBX_TRACE_WARNING(
                "Destroy entry point not found in plugin module '{}': {}",
                lib->get_path().string(),
                "tbx_destroy_plugin");
            return {};
        }

        Plugin* plugin_instance = create();
        if (!plugin_instance)
        {
            TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name);
            return {};
        }

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(plugin_instance, destroy);
        return LoadedPlugin(meta, std::move(lib), std::move(instance));
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

    static bool is_windowing_plugin(const PluginMeta& meta)
    {
        return to_lower(meta.name).find("window") != std::string::npos;
    }

    static bool should_unload_before(const PluginMeta& left, const PluginMeta& right)
    {
        const bool left_is_logging = left.category == PluginCategory::LOGGING;
        const bool right_is_logging = right.category == PluginCategory::LOGGING;
        if (left_is_logging != right_is_logging)
            return !left_is_logging;

        const bool left_is_windowing = is_windowing_plugin(left);
        const bool right_is_windowing = is_windowing_plugin(right);
        const bool left_is_rendering = left.category == PluginCategory::RENDERING;
        const bool right_is_rendering = right.category == PluginCategory::RENDERING;
        if (left_is_rendering && right_is_windowing)
            return true;
        if (right_is_rendering && left_is_windowing)
            return false;

        if (left.priority != right.priority)
            return left.priority > right.priority;

        return to_lower(left.name) < to_lower(right.name);
    }

    static uint32 get_update_category_rank(PluginCategory category, bool is_fixed_update)
    {
        (void)is_fixed_update;
        switch (category)
        {
            case PluginCategory::LOGGING:
                return 0U;
            case PluginCategory::DEFAULT:
                return 1U;
            case PluginCategory::INPUT:
                return 2U;
            case PluginCategory::AUDIO:
                return 3U;
            case PluginCategory::GAMEPLAY:
                return 4U;
            case PluginCategory::PHYSICS:
                return 5U;
            case PluginCategory::RENDERING:
                return 6U;
            default:
                return 7U;
        }
    }

    static std::vector<size> build_update_order(
        const std::vector<LoadedPlugin>& loaded_plugins,
        bool is_fixed_update)
    {
        auto ordered_indices = std::vector<size>(loaded_plugins.size(), size {0});
        std::iota(ordered_indices.begin(), ordered_indices.end(), size {0});

        std::stable_sort(
            ordered_indices.begin(),
            ordered_indices.end(),
            [&loaded_plugins, is_fixed_update](size left_index, size right_index)
            {
                const auto& left = loaded_plugins[left_index].meta;
                const auto& right = loaded_plugins[right_index].meta;

                uint32 left_rank = get_update_category_rank(left.category, is_fixed_update);
                uint32 right_rank = get_update_category_rank(right.category, is_fixed_update);
                if (left_rank != right_rank)
                    return left_rank < right_rank;

                if (left.priority != right.priority)
                    return left.priority < right.priority;

                return to_lower(left.name) < to_lower(right.name);
            });

        return ordered_indices;
    }

}
