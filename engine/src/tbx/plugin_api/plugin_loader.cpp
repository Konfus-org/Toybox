#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/debug/macros.h"
#include "tbx/strings/mods.h"
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tbx
{
    static std::filesystem::path resolve_module_path(const PluginMeta& meta)
    {
        std::filesystem::path module = meta.module_path;

        if (module.empty())
        {
            module = meta.root_directory;
        }

        // If module is a directory, use plugin name as base name.
        if (std::filesystem::is_directory(module))
        {
            module /= meta.name; // base name fallback
        }

        // If no extension, add platform-specific extension and unix lib prefix
        if (module.extension().empty())
        {
#if defined(TBX_PLATFORM_WINDOWS)
            module += ".dll";
#elif defined(TBX_PLATFORM_MACOS)
            if (module.filename().string().rfind("lib", 0) != 0)
                module = module.parent_path() / (std::string("lib") + module.filename().string());
            module += ".dylib";
#else
            if (module.filename().string().rfind("lib", 0) != 0)
                module = module.parent_path() / (std::string("lib") + module.filename().string());
            module += ".so";
#endif
        }

        return module;
    }

    static std::optional<LoadedPlugin> load_plugin(const PluginMeta& meta)
    {
        LoadedPlugin loaded;
        loaded.meta = meta;

        if (meta.linkage == PluginLinkage::Static)
        {
            Plugin* plug = PluginRegistry::get_instance().find_plugin(meta.name);
            if (!plug)
            {
                TBX_TRACE_WARNING("Static plugin not registered: {}", meta.name.c_str());
                return std::nullopt;
            }

            auto instance = PluginInstance(plug, [](Plugin*) {});
            loaded.instance = std::move(instance);
            return loaded;
        }

        const std::filesystem::path module_path = resolve_module_path(meta);
        Scope<SharedLibrary> lib = make_scope<SharedLibrary>(module_path);

        const std::string create_symbol = "create_" + meta.name;
        CreatePluginFn create = lib->get_symbol<CreatePluginFn>(create_symbol.c_str());
        if (!create)
        {
            TBX_TRACE_WARNING("Entry point not found in plugin module: {}", create_symbol.c_str());
            return std::nullopt;
        }

        const std::string destroy_symbol = "destroy_" + meta.name;
        DestroyPluginFn destroy = lib->get_symbol<DestroyPluginFn>(destroy_symbol.c_str());
        if (!destroy)
        {
            TBX_TRACE_WARNING(
                "Destroy entry point not found in plugin module: {}",
                destroy_symbol.c_str());
            return std::nullopt;
        }

        Plugin* plugin_instance = create();
        if (!plugin_instance)
        {
            TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name.c_str());
            return std::nullopt;
        }

        PluginInstance instance(plugin_instance, PluginDeleter(destroy));
        loaded.instance = std::move(instance);
        loaded.library = std::move(lib);
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        // The first pass walks every manifest to build an index before we know
        // which plugins will ultimately be requested.
        if (std::filesystem::exists(directory))
        {
            for (auto& entry : std::filesystem::recursive_directory_iterator(directory))
            {
                if (!entry.is_regular_file())
                    continue;
                auto p = entry.path();
                const std::string name = p.filename().string();
                if (p.extension() == ".meta" || to_lower_case_string(name) == "plugin.meta")
                {
                    try
                    {
                        PluginMeta m = parse_plugin_meta(p);
                        discovered.push_back(std::move(m));
                    }
                    catch (...)
                    {
                        const std::string manifest_path = entry.path().string();
                        TBX_TRACE_WARNING(
                            "Plugin {} is unable to be loaded!",
                            manifest_path.c_str());
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
            metas = std::move(discovered);
        }
        else
        {
            // Build lookup tables keyed by plugin id and by plugin type so we
            // can resolve dependencies expressed as either kind of token.
            std::unordered_map<std::string, size_t> by_name_lookup;
            std::unordered_map<std::string, std::vector<size_t>> by_type;
            by_name_lookup.reserve(discovered.size());
            by_type.reserve(discovered.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                const PluginMeta& meta = discovered[index];
                by_name_lookup.emplace(to_lower_case_string(meta.name), index);
                if (!meta.type.empty())
                {
                    by_type[to_lower_case_string(meta.type)].push_back(index);
                }
            }

            // Track the index of every manifest that we have chosen to load.
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
                std::string needle = to_lower_case_string(trim_string(token));
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
                std::string needle = to_lower_case_string(trim_string(requested));
                auto it = by_name_lookup.find(needle);
                if (it != by_name_lookup.end())
                {
                    enqueue_index(it->second);
                }
            }

            while (!pending.empty())
            {
                size_t index = pending.front();
                pending.pop_front();
                const PluginMeta& meta = discovered[index];
                // Hard dependencies are mandatory. Their tokens can be ids or
                // types, so resolve each one through the lookup tables.
                for (const std::string& dependency : meta.dependencies)
                {
                    enqueue_dependency_token(dependency);
                }
            }

            metas.reserve(selected.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                if (selected.find(index) != selected.end())
                {
                    metas.push_back(discovered[index]);
                }
            }
        }

        std::vector<PluginMeta> ordered = resolve_plugin_load_order(metas);
        auto from_mem = load_plugins(ordered);
        for (auto& lp : from_mem)
            loaded.push_back(std::move(lp));
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(const std::vector<PluginMeta>& metas)
    {
        std::vector<LoadedPlugin> loaded;
        for (const PluginMeta& meta : metas)
        {
            auto plugin = load_plugin(meta);
            if (!plugin)
                continue;

            loaded.push_back(std::move(*plugin));
        }
        return loaded;
    }
}
