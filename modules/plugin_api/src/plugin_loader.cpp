#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/common/smart_pointers.h"
#include "tbx/common/string_extensions.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/filesystem.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_registry.h"
#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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
                module_path =
                    module_path.parent_path().append(std::string("lib") + file_name.std_str());
            }
            module_path = module_path.set_extension(".dylib");
#else
            const String file_name = module_path.filename_string();
            if (!file_name.starts_with("lib"))
            {
                module_path =
                    module_path.parent_path().append(std::string("lib") + file_name.std_str());
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

        const std::string create_symbol = "create_" + meta.name;
        CreatePluginFn create = lib->get_symbol<CreatePluginFn>(create_symbol.c_str());
        if (!create)
        {
            TBX_TRACE_WARNING("Entry point not found in plugin module: {}", create_symbol.c_str());
            return {};
        }

        const std::string destroy_symbol = "destroy_" + meta.name;
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

        TBX_TRACE_INFO("Loaded plugin: {}", to_string(loaded));

        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const FilePath& directory,
        const std::vector<std::string>& requested_ids,
        IFileSystem& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        if (file_ops.exists(directory))
        {
            for (const auto& entry : file_ops.read_directory(directory))
            {
                if (file_ops.get_file_type(entry) != FilePathType::Regular)
                {
                    continue;
                }

                const String name = entry.filename_string();
                const std::string lowered_name = to_lower_case_string(name);
                if (entry.get_extension().std_str() == ".meta" || lowered_name == "plugin.meta")
                {
                    try
                    {
                        std::string manifest_data;
                        if (!file_ops.read_file(entry, manifest_data, FileDataFormat::Utf8Text))
                        {
                            continue;
                        }

                        PluginMeta m = parse_plugin_meta(manifest_data, entry);
                        discovered.push_back(m);
                    }
                    catch (...)
                    {
                        TBX_TRACE_WARNING(
                            "Plugin {} is unable to be loaded!",
                            entry.std_path().string().c_str());
                    }
                }
            }
        }

        if (discovered.empty())
        {
            return loaded;
        }

        std::vector<PluginMeta> metas;
        if (requested_ids.empty())
        {
            metas = discovered;
        }
        else
        {
            std::unordered_map<std::string, size_t> by_name_lookup;
            std::unordered_map<std::string, std::vector<size_t>> by_type;
            by_name_lookup.reserve(discovered.size());
            by_type.reserve(discovered.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                const PluginMeta& meta = discovered[index];
                const std::string lowered_name = to_lower_case_string(meta.name);
                by_name_lookup.emplace(lowered_name, index);
                if (!meta.type.empty())
                {
                    const std::string lowered_type = to_lower_case_string(meta.type);
                    by_type[lowered_type].push_back(index);
                }
            }

            std::unordered_set<size_t> selected;
            std::deque<size_t> pending;

            auto enqueue_index = [&](size_t index)
            {
                if (selected.insert(index).second)
                {
                    pending.push_back(index);
                }
            };

            auto enqueue_dependency_token = [&](const std::string& token)
            {
                const std::string trimmed = trim_string(token);
                const std::string needle = to_lower_case_string(trimmed);
                if (needle.empty())
                {
                    return;
                }

                auto id_it = by_name_lookup.find(needle);
                if (id_it != by_name_lookup.end())
                {
                    enqueue_index(id_it->second);
                }

                auto type_it = by_type.find(needle);
                if (type_it != by_type.end())
                {
                    for (size_t candidate : type_it->second)
                    {
                        enqueue_index(candidate);
                    }
                }
            };

            for (const std::string& requested : requested_ids)
            {
                enqueue_dependency_token(requested);
            }

            while (!pending.empty())
            {
                size_t index = pending.front();
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
            return loaded;
        }

        metas = resolve_plugin_load_order(metas);
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

    std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        IFileSystem& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

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
