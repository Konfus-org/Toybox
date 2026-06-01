#include "tbx/systems/plugin_api/plugin_loader.h"
#include "tbx/interfaces/file_ops.h"
#include "tbx/interfaces/plugin.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/plugin_api/plugin_ownership.h"
#include "tbx/utils/string_utils.h"
#include <atomic>
#include <chrono>
#include <numeric>
#include <system_error>
#include <thread>

namespace tbx
{
    inline constexpr std::string_view PluginShadowCopyDirectory = ".plugin_load_copies";
    inline constexpr std::string_view PluginShadowCopyMarker = ".load_copy";
    static std::atomic_uint32_t g_plugin_meta_query_depth = 0U;

    bool is_plugin_meta_query_active()
    {
        return g_plugin_meta_query_depth.load(std::memory_order_relaxed) > 0U;
    }

    struct PluginMetaQueryScope final
    {
        PluginMetaQueryScope()
        {
            g_plugin_meta_query_depth.fetch_add(1U, std::memory_order_relaxed);
        }

        ~PluginMetaQueryScope() noexcept
        {
            g_plugin_meta_query_depth.fetch_sub(1U, std::memory_order_relaxed);
        }
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

    bool try_query_plugin_meta_from_library(
        const std::filesystem::path& library_path,
        IFileOps& file_ops,
        PluginMeta& out_meta)
    {
        const auto query_scope = PluginMetaQueryScope {};
        static_cast<void>(query_scope);
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
        RegisterPluginScriptsFn register_scripts =
            lib->get_symbol<RegisterPluginScriptsFn>("tbx_register_plugin_scripts");
        UnregisterPluginScriptsFn unregister_scripts =
            lib->get_symbol<UnregisterPluginScriptsFn>("tbx_unregister_plugin_scripts");
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
            return {};
        }

        auto instance = std::unique_ptr<Plugin, PluginDeleter>(
            plugin_instance,
            [destroy, unregister_scripts, plugin_id](Plugin* plugin_ptr)
            {
                auto destroy_scope = ScopedPluginContext(plugin_id);
                if (unregister_scripts)
                    unregister_scripts();
                destroy(plugin_ptr);
            });
        if (register_scripts)
            register_scripts();
        auto loaded =
            LoadedPlugin(meta, std::move(lib), std::move(instance), register_services, bind_runtime);
        loaded.set_id(plugin_id);
        return loaded;
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
            const std::filesystem::path debug_candidate = append_debug_postfix(library_path);
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
                if (path_contains_directory_token(entry, "resources"))
                    continue;
                if (path_contains_directory_token(entry, PluginShadowCopyDirectory))
                    continue;
                if (is_plugin_shadow_copy_path(entry))
                    continue;

                if (file_ops.get_type(entry) != FileType::FILE)
                    continue;

                if (!is_plugin_library_path(entry))
                    continue;

                auto meta = PluginMeta {};
                if (try_query_plugin_meta_from_library(entry, file_ops, meta))
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

        metas = resolve_plugin_load_order(metas);
        if (metas.empty())
            return loaded;
        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops);
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
            LoadedPlugin plug = load_plugin_internal(meta, file_ops);
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
        auto ordered_indices = build_update_order(loaded_plugins, false);
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
        auto ordered_indices = build_update_order(loaded_plugins, true);
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
                    return should_unload_before(left, right);
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
