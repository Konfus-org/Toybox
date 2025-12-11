#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/common/collections.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_registry.h"
#include <deque>
#include <utility>

namespace tbx
{
    static FilePath resolve_module_path(const PluginMeta& meta, IFileSystem& file_ops)
    {
        FilePath module_path = meta.module_path;

        if (module_path.empty())
        {
            module_path = meta.root_directory;
        }

        if (file_ops.get_file_type(module_path) == FilePathType::Directory)
        {
            module_path = module_path.append(meta.name);
        }

        if (module_path.get_extension().empty())
        {
#if defined(TBX_PLATFORM_WINDOWS)
            module_path = module_path.set_extension(".dll");
#elif defined(TBX_PLATFORM_MACOS)
            const String file_name = module_path.filename_string();
            if (!file_name.starts_with("lib"))
            {
                module_path = module_path.parent_path().append(String("lib") + file_name.std_str());
            }
            module_path = module_path.set_extension(".dylib");
#else
            const String file_name = module_path.filename_string();
            if (!file_name.starts_with("lib"))
            {
                module_path = module_path.parent_path().append(String("lib") + file_name.std_str());
            }
            module_path = module_path.set_extension(".so");
#endif
        }

        return module_path;
    }

    static LoadedPlugin load_plugin_internal(const PluginMeta& meta, IFileSystem& file_ops)
    {
        LoadedPlugin loaded;
        loaded.meta = meta;

        if (meta.linkage == PluginLinkage::Static)
        {
            Plugin* plug = PluginRegistry::get_instance().find_plugin(meta.name);
            if (!plug)
            {
                TBX_TRACE_WARNING("Static plugin not registered: {}", meta.name.c_str());
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

        const FilePath module_path = resolve_module_path(meta, file_ops);
        auto lib = Scope<SharedLibrary>(module_path);

        const String create_symbol = String("create_") + meta.name;
        CreatePluginFn create = lib->get_symbol<CreatePluginFn>(create_symbol.c_str());
        if (!create)
        {
            TBX_TRACE_WARNING("Entry point not found in plugin module: {}", create_symbol.c_str());
            return {};
        }

        const String destroy_symbol = String("destroy_") + meta.name;
        DestroyPluginFn destroy = lib->get_symbol<DestroyPluginFn>(destroy_symbol.c_str());
        if (!destroy)
        {
            TBX_TRACE_WARNING(
                "Destroy entry point not found in plugin module: {}",
                destroy_symbol.c_str());
            return {};
        }

        Plugin* plugin_instance = create();
        if (!plugin_instance)
        {
            TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name.c_str());
            return {};
        }

        auto instance = PluginInstance(plugin_instance, destroy);
        loaded.instance = std::move(instance);
        loaded.library = std::move(lib);

        TBX_TRACE_INFO("Loaded plugin: {}", String(loaded));

        return loaded;
    }

    static List<PluginMeta> resolve_plugin_load_order(const List<PluginMeta>& plugins)
    {
        HashMap<String, size_t> by_name_lookup;
        by_name_lookup.reserve(plugins.size());
        for (size_t index = 0; index < plugins.size(); ++index)
        {
            by_name_lookup.emplace(plugins[index].name.to_lower(), index);
        }

        List<List<size_t>> dependencies(plugins.size());
        for (size_t index = 0; index < plugins.size(); ++index)
        {
            HashSet<size_t> unique;
            for (const String& dependency : plugins[index].dependencies)
            {
                const String needle = dependency.trim().to_lower();
                auto it = by_name_lookup.find(needle);
                if (it == by_name_lookup.end() || it->second == index)
                {
                    TBX_ASSERT(
                        false,
                        "Failed to resolve dependency '{}' for '{}'",
                        dependency.c_str(),
                        plugins[index].name.c_str());
                    return {};
                }

                if (unique.insert(it->second).second)
                {
                    dependencies[index].push_back(it->second);
                }
            }
        }

        List<size_t> indegree(plugins.size(), 0);
        List<List<size_t>> adjacency(plugins.size());
        for (size_t index = 0; index < plugins.size(); ++index)
        {
            for (size_t dependency : dependencies[index])
            {
                adjacency[dependency].push_back(index);
                indegree[index] += 1;
            }
        }

        std::deque<size_t> ready;
        for (size_t index = 0; index < plugins.size(); ++index)
        {
            if (indegree[index] == 0)
            {
                ready.push_back(index);
            }
        }

        List<PluginMeta> ordered;
        ordered.reserve(plugins.size());
        while (!ready.empty())
        {
            const size_t current = ready.front();
            ready.pop_front();
            ordered.push_back(plugins[current]);

            for (size_t dependent : adjacency[current])
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

    List<LoadedPlugin> load_plugins(
        const FilePath& directory,
        const List<String>& requested_ids,
        IFileSystem& file_ops)
    {
        List<LoadedPlugin> loaded;

        List<PluginMeta> discovered;
        if (file_ops.exists(directory))
        {
            for (const auto& entry : file_ops.read_directory(directory))
            {
                if (file_ops.get_file_type(entry) != FilePathType::Regular)
                {
                    continue;
                }

                const String name = entry.filename_string();
                const String lowered_name = name.to_lower();
                if (entry.get_extension().std_str() == ".meta"
                    || lowered_name == String("plugin.meta"))
                {
                    String manifest_data;
                    if (!file_ops.read_file(entry, manifest_data, FileDataFormat::Utf8Text))
                        continue;

                    PluginMeta manifest_meta;
                    if (try_parse_plugin_meta(manifest_data, entry, manifest_meta))
                    {
                        discovered.push_back(manifest_meta);
                    }
                    else
                    {
                        const String manifest_path = entry.std_path().string();
                        TBX_ASSERT(
                            false,
                            "Plugin {} is unable to be loaded!",
                            manifest_path);
                    }
                }
            }
        }

        if (discovered.empty())
        {
            return loaded;
        }

        List<PluginMeta> metas;
        if (requested_ids.empty())
        {
            metas = discovered;
        }
        else
        {
            HashMap<String, size_t> by_name_lookup;
            by_name_lookup.reserve(discovered.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                const String lowered_name = discovered[index].name.to_lower();
                by_name_lookup.emplace(lowered_name, index);
            }

            HashSet<size_t> selected;
            std::deque<size_t> pending;

            auto enqueue_index = [&](size_t index)
            {
                if (selected.insert(index).second)
                {
                    pending.push_back(index);
                }
            };

            auto enqueue_dependency_token = [&](const String& token)
            {
                const String trimmed = token.trim();
                const String needle = trimmed.to_lower();
                if (needle.empty())
                {
                    return;
                }

                auto id_it = by_name_lookup.find(needle);
                if (id_it != by_name_lookup.end())
                {
                    enqueue_index(id_it->second);
                }
                else
                {
                    TBX_ASSERT(false, "Requested plugin not found: {}", trimmed.c_str());
                }
            };

            for (const String& requested : requested_ids)
            {
                enqueue_dependency_token(requested);
            }

            while (!pending.empty())
            {
                size_t index = pending.front();
                pending.pop_front();
                metas.push_back(discovered[index]);
                for (const String& dependency : discovered[index].dependencies)
                {
                    enqueue_dependency_token(dependency);
                }
            }
        }

        if (metas.empty())
        {
            return loaded;
        }

        metas = resolve_plugin_load_order(metas);
        if (metas.empty())
        {
            return loaded;
        }
        for (const PluginMeta& meta : metas)
        {
            LoadedPlugin plug = load_plugin_internal(meta, file_ops);
            if (plug.instance)
            {
                loaded.push_back(std::move(plug));
            }
        }

        return loaded;
    }

    List<LoadedPlugin> load_plugins(const List<PluginMeta>& metas, IFileSystem& file_ops)
    {
        List<LoadedPlugin> loaded;

        for (const PluginMeta& meta : metas)
        {
            auto plug = load_plugin_internal(meta, file_ops);
            if (plug.instance)
            {
                loaded.push_back(std::move(plug));
            }
        }

        return loaded;
    }
}
