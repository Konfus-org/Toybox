#include "plugin_loader.h"
#include "plugin_loader_discovery.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/assets/serialization.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/utils/string_utils.h"
#include <algorithm>
#include <chrono>
#include <system_error>
#include <thread>

namespace tbx
{
    inline constexpr std::string_view PluginShadowCopyDirectory = ".plugin_load_copies";
    inline constexpr std::string_view PluginShadowCopyMarker = ".load_copy";

    static bool is_plugin_library_path(const std::filesystem::path& path);
    static bool is_plugin_meta_path(const std::filesystem::path& path);
    static std::filesystem::path get_plugin_library_path_from_meta_path(
        const std::filesystem::path& meta_path);
    static std::filesystem::path get_plugin_meta_path_from_library_path(
        const std::filesystem::path& library_path);
    static std::filesystem::path resolve_plugin_library_path(
        const PluginMeta& meta,
        IFileOps& file_ops);
    static bool try_query_plugin_meta_from_file(
        const std::filesystem::path& meta_path,
        IFileOps& file_ops,
        PluginMeta& out_meta);

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

    static std::filesystem::path make_plugin_shadow_copy_path(
        const std::filesystem::path& library_path)
    {
        const auto copy_stem = library_path.stem().string();
        const auto copy_name =
            copy_stem + std::string(PluginShadowCopyMarker) + library_path.extension().string();
        return library_path.parent_path() / copy_name;
    }

    static bool is_plugin_shadow_copy_path(const std::filesystem::path& library_path)
    {
        if (path_contains_directory_token(library_path, PluginShadowCopyDirectory))
            return true;

        return library_path.stem().string().find(PluginShadowCopyMarker) != std::string::npos;
    }

    static std::filesystem::path try_create_plugin_shadow_copy(
        const std::filesystem::path& library_path,
        IFileOps& file_ops)
    {
        if (file_ops.get_type(library_path) != FileType::FILE)
            return {};

        if (is_plugin_shadow_copy_path(library_path))
            return {};

        const auto shadow_copy_path = make_plugin_shadow_copy_path(library_path);
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
                return shadow_copy_path;

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
            cleanup_path = shadow_copy_path;
        }
#endif

        auto lib = load_shared_lib(load_path, cleanup_path);
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
            lib = load_shared_lib(load_path, cleanup_path);
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

    static bool try_query_plugin_meta_from_file(
        const std::filesystem::path& meta_path,
        IFileOps& file_ops,
        PluginMeta& out_meta)
    {
        if (file_ops.get_type(meta_path) != FileType::FILE)
            return false;

        auto contents = std::string {};
        if (!file_ops.read_file(meta_path, FileDataFormat::UTF8_TEXT, contents))
            return false;

        auto meta = PluginMeta {};
        if (!read_json_serializable_value(contents, meta))
            return false;

        const auto library_path = get_plugin_library_path_from_meta_path(meta_path);
        if (meta.name.empty() || meta.version.empty() || library_path.empty())
            return false;

        meta.root_directory = library_path.parent_path();
        meta.library_path = library_path;
        meta.linkage = PluginLinkage::DYNAMIC;
        out_meta = std::move(meta);
        return true;
    }

    static bool try_emplace_loaded_plugin(
        LoadedPlugins& out_plugins,
        const PluginMeta& meta,
        IFileOps& file_ops)
    {
        if (meta.abi_version != PluginAbiVersion)
        {
            TBX_TRACE_WARNING(
                "Plugin ABI mismatch for {}: expected {}, found {}",
                meta.name,
                PluginAbiVersion,
                meta.abi_version);
            return false;
        }

        const std::filesystem::path library_path =
            resolve_plugin_library_path(meta, file_ops);
        auto lib = load_plugin_library(library_path, file_ops);
        if (!lib || !lib->is_valid())
            return false;

        CreatePluginFn create = lib->get_symbol<CreatePluginFn>("tbx_create_plugin");
        if (!create)
        {
            TBX_TRACE_WARNING(
                "Entry point not found in plugin module '{}': {}",
                lib->get_path().string(),
                "tbx_create_plugin");
            return false;
        }

        DestroyPluginFn destroy = lib->get_symbol<DestroyPluginFn>("tbx_destroy_plugin");
        if (!destroy)
        {
            TBX_TRACE_WARNING(
                "Destroy entry point not found in plugin module '{}': {}",
                lib->get_path().string(),
                "tbx_destroy_plugin");
            return false;
        }

        RegisterPluginServicesFn register_services =
            lib->get_symbol<RegisterPluginServicesFn>("tbx_register_plugin_services");
        BindPluginRuntimeFn bind_runtime =
            lib->get_symbol<BindPluginRuntimeFn>("tbx_bind_plugin_runtime");

        const auto plugin_id = allocate_plugin_instance_id();
        auto plugin_scope = ScopedPluginContext(plugin_id);
        Plugin* plugin_instance = create();
        if (!plugin_instance)
        {
            TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name);
            return false;
        }

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(
            plugin_instance,
            [destroy, plugin_id](Plugin* plugin_ptr)
            {
                auto destroy_scope = ScopedPluginContext(plugin_id);
                destroy(plugin_ptr);
            });

        auto& loaded = out_plugins.emplace_back(
            meta,
            std::move(lib),
            std::move(instance),
            register_services,
            bind_runtime);
        loaded.set_id(plugin_id);

        return true;
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

    static bool is_plugin_library_path(const std::filesystem::path& path)
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

    static std::filesystem::path resolve_plugin_library_path(
        const PluginMeta& meta,
        IFileOps& file_ops)
    {
        std::filesystem::path library_path = meta.library_path;

        if (library_path.empty())
            library_path = meta.root_directory;

        if (file_ops.get_type(library_path) == FileType::DIRECTORY)
            library_path /= meta.name;

        return resolve_shared_library_path(file_ops.resolve(library_path));
    }

    static bool is_plugin_meta_path(const std::filesystem::path& path)
    {
        if (to_lower(path.extension().string()) != ".meta")
            return false;

        return is_plugin_library_path(get_plugin_library_path_from_meta_path(path));
    }

    static std::filesystem::path get_plugin_library_path_from_meta_path(
        const std::filesystem::path& meta_path)
    {
        if (to_lower(meta_path.extension().string()) != ".meta")
            return {};

        return meta_path.parent_path() / meta_path.stem();
    }

    static std::filesystem::path get_plugin_meta_path_from_library_path(
        const std::filesystem::path& library_path)
    {
        if (!is_plugin_library_path(library_path))
            return {};

        auto meta_path = library_path;
        meta_path += ".meta";
        return meta_path;
    }

    LoadedPlugins PluginLoader::load(
        const std::filesystem::path& path,
        const std::vector<std::string>& requested_ids)
    {
        auto default_file_ops = FileOperator();
        return load(path, requested_ids, default_file_ops);
    }

    LoadedPlugins PluginLoader::load(
        const std::filesystem::path& path,
        const std::vector<std::string>& requested_ids,
        IFileOps& file_ops)
    {
        auto loaded = LoadedPlugins();
        const auto resolved_path = path.empty() ? file_ops.get_working_directory() : path;

        std::vector<PluginMeta> discovered;
        const auto path_type = file_ops.get_type(resolved_path);
        const bool is_single_library_load = path_type == FileType::FILE;
        const bool has_requested_ids = !requested_ids.empty();
        if (path_type == FileType::FILE)
        {
            auto meta = PluginMeta {};
            if (!is_plugin_shadow_copy_path(resolved_path) && is_plugin_meta_path(resolved_path))
            {
                if (try_query_plugin_meta_from_file(resolved_path, file_ops, meta))
                    discovered.push_back(std::move(meta));
            }
            else if (
                !is_plugin_shadow_copy_path(resolved_path) && is_plugin_library_path(resolved_path))
            {
                if (try_query_plugin_meta_from_file(
                        get_plugin_meta_path_from_library_path(resolved_path),
                        file_ops,
                        meta))
                    discovered.push_back(std::move(meta));
            }
        }
        else if (path_type == FileType::DIRECTORY && has_requested_ids)
        {
            auto queued_names = std::deque<std::string> {};
            auto queued_lookup = std::unordered_set<std::string> {};
            auto discovered_lookup = std::unordered_set<std::string> {};

            const auto enqueue_plugin_name =
                [&queued_names, &queued_lookup](const std::string& plugin_name)
            {
                const auto trimmed_name = trim(plugin_name);
                const auto lowered_name = to_lower(trimmed_name);
                if (lowered_name.empty() || queued_lookup.contains(lowered_name))
                    return;

                queued_names.push_back(trimmed_name);
                queued_lookup.insert(lowered_name);
            };

            for (const auto& requested_id : requested_ids)
                enqueue_plugin_name(requested_id);

            while (!queued_names.empty())
            {
                const auto plugin_name = queued_names.front();
                queued_names.pop_front();

                const auto library_path =
                    resolve_requested_plugin_library_path(resolved_path, plugin_name, file_ops);
                if (library_path.empty())
                {
                    TBX_TRACE_WARNING("Requested plugin not found: {}", trim(plugin_name));
                    continue;
                }

                auto meta = PluginMeta {};
                if (!try_query_plugin_meta_from_file(
                        get_plugin_meta_path_from_library_path(library_path),
                        file_ops,
                        meta))
                    continue;

                const auto lowered_name = to_lower(meta.name);
                if (discovered_lookup.contains(lowered_name))
                    continue;

                discovered_lookup.insert(lowered_name);
                for (const auto& dependency : meta.dependencies)
                    enqueue_plugin_name(dependency);

                discovered.push_back(std::move(meta));
            }
        }
        else if (path_type == FileType::DIRECTORY)
        {
            for (const std::filesystem::path& entry : file_ops.read_directory(resolved_path))
            {
                // Keep plugin discovery from crawling bundled assets or loader-owned shadow
                // copies near the executable.
                if (path_contains_directory_token(entry, "resources"))
                    continue;
                if (path_contains_directory_token(entry, PluginShadowCopyDirectory))
                    continue;
                if (is_plugin_shadow_copy_path(entry))
                    continue;

                if (file_ops.get_type(entry) != FileType::FILE)
                    continue;

                if (!is_plugin_meta_path(entry))
                    continue;

                auto meta = PluginMeta();
                if (try_query_plugin_meta_from_file(entry, file_ops, meta))
                    discovered.push_back(std::move(meta));
            }
        }

        if (discovered.empty())
        {
            TBX_TRACE_WARNING("No plugins found at {}", resolved_path.string());
            return loaded;
        }

        std::vector<PluginMeta> metas;
        if (!has_requested_ids)
        {
            metas = discovered;
        }
        else if (is_single_library_load)
        {
            const auto requested_match = std::ranges::any_of(
                requested_ids,
                [&discovered](const std::string& requested_id)
                {
                    return !discovered.empty()
                           && to_lower(trim(requested_id)) == to_lower(discovered.front().name);
                });
            if (requested_match)
                metas = discovered;
        }
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

        if (!is_single_library_load)
        {
            metas = resolve_plugin_load_order(metas);
            if (metas.empty())
                return loaded;
        }
        for (const PluginMeta& meta : metas)
        {
            if (!try_emplace_loaded_plugin(loaded, meta, file_ops))
                TBX_TRACE_WARNING("Failed to load plugin: {}", meta.name);
        }

        return loaded;
    }

}
