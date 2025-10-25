#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/strings/string_utils.h"
#include <fstream>

namespace tbx
{
    using CreatePluginFn = Plugin* (*)();

    static std::string to_lower_copy(const std::string& s)
    {
        return to_lower_case_string(s);
    }

    static bool id_in_requests(const std::string& id, const std::unordered_set<std::string>& requests_lower)
    {
        return requests_lower.find(to_lower_case_string(id)) != requests_lower.end();
    }

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
        std::unordered_set<std::string> req;
        for (const auto& id : requested_ids) req.insert(to_lower_copy(id));

        std::vector<PluginMeta> metas;
        if (std::filesystem::exists(directory))
        {
            for (auto& entry : std::filesystem::recursive_directory_iterator(directory))
            {
                if (!entry.is_regular_file()) continue;
                auto p = entry.path();
                const std::string name = p.filename().string();
                if (p.extension() == ".meta" || to_lower_case_string(name) == "plugin.meta")
                {
                    try {
                        PluginMeta m = parse_plugin_meta(p);
                        if (req.empty() || id_in_requests(m.id, req))
                        {
                            metas.push_back(std::move(m));
                        }
                    } catch (...) {
                        // ignore malformed manifests in this pass
                    }
                }
            }
        }

        if (metas.empty())
        {
            return loaded;
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
            const std::filesystem::path module_path = resolve_module_path(meta);
            auto lib = make_scope<SharedLibrary>(module_path);

            CreatePluginFn create = lib->get_symbol<CreatePluginFn>(meta.entry_point.c_str());
            if (!create)
            {
                throw std::runtime_error("Entry point not found in plugin module: " + meta.entry_point);
            }

            Plugin* raw = create();
            if (!raw)
            {
                throw std::runtime_error("Plugin factory returned null for: " + meta.id);
            }

            LoadedPlugin lp{};
            lp.meta = meta;
            lp.is_dynamic = true;
            lp.library = std::move(lib);
            lp.instance.reset(raw);
            loaded.push_back(std::move(lp));
        }
        return loaded;
    }
}
