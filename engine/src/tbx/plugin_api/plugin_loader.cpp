#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/strings/string_utils.h"
#include "tbx/debug/log_macros.h"
#include <deque>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

namespace tbx
{
    static std::filesystem::path resolve_module_path(const PluginMeta& meta)
    {
        std::filesystem::path module = meta.module_path;

        if (module.empty())
        {
            module = meta.root_directory;
        }

        // If module is a directory, use entry_point as base name.
        if (std::filesystem::is_directory(module))
        {
            module /= meta.entry_point; // base name fallback
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
                if (!entry.is_regular_file()) continue;
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
                            "Plugin {} is unable to be loaded!", manifest_path.c_str());
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
            std::unordered_map<std::string, size_t> by_id;
            std::unordered_map<std::string, std::vector<size_t>> by_type;
            by_id.reserve(discovered.size());
            by_type.reserve(discovered.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                const PluginMeta& meta = discovered[index];
                by_id.emplace(to_lower_case_string(meta.id), index);
                if (!meta.type.empty())
                {
                    by_type[to_lower_case_string(meta.type)].push_back(index);
                }
            }

            // Track the index of every manifest that we have chosen to load.
            std::unordered_set<size_t> selected;
            std::deque<size_t> pending;

            auto enqueue_index = [&](size_t index) {
                if (selected.insert(index).second)
                {
                    pending.push_back(index);
                }
            };

            auto enqueue_dependency_token = [&](const std::string& token) {
                std::string needle = to_lower_case_string(trim_string(token));
                if (needle.empty())
                {
                    return;
                }

                auto id_it = by_id.find(needle);
                if (id_it != by_id.end())
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
                auto it = by_id.find(needle);
                if (it != by_id.end())
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
        for (auto& lp : from_mem) loaded.push_back(std::move(lp));
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(const std::vector<PluginMeta>& metas)
    {
        std::vector<LoadedPlugin> loaded;
        for (const PluginMeta& meta : metas)
        {
            Plugin* raw = nullptr;
            DestroyPluginFn destroy = nullptr;
            Scope<SharedLibrary> lib;

            if (meta.linkage == PluginLinkage::Static)
            {
                auto entry = PluginRegistry::instance().find_static_plugin_entry(meta.entry_point);
                if (!entry || !entry->create)
                {
                    TBX_TRACE_WARNING(
                        "Static plugin entry point not registered: {}", meta.entry_point.c_str());
                    continue;
                }

                destroy = entry->destroy;
                raw = entry->create();
            }
            else
            {
                const std::filesystem::path module_path = resolve_module_path(meta);
                lib = make_scope<SharedLibrary>(module_path);

                CreatePluginFn create = lib->get_symbol<CreatePluginFn>(meta.entry_point.c_str());
                if (!create)
                {
                    TBX_TRACE_WARNING(
                        "Entry point not found in plugin module: {}", meta.entry_point.c_str());
                    continue;
                }

                std::string destroy_name = meta.entry_point + "_Destroy";
                destroy = lib->get_symbol<DestroyPluginFn>(destroy_name.c_str());
                if (!destroy)
                {
                    TBX_TRACE_WARNING(
                        "Destroy entry point not found in plugin module: {}", destroy_name.c_str());
                }

                raw = create();
            }

            if (!raw)
            {
                TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.id.c_str());
                continue;
            }

            LoadedPlugin lp;
            lp.meta = meta;
            lp.library = std::move(lib);
            lp.instance = PluginInstance(raw, PluginDeleter{ destroy });
            loaded.push_back(std::move(lp));
        }
        return loaded;
    }
}
