#include "tbx/plugin_api/plugin_loader.h"
#include "tbx/debugging/macros.h"
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/std/smart_pointers.h"
#include "tbx/std/string.h"
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

static std::string tbx_copy_string(const tbx::String& value)
{
    return std::string(value.get_raw(), value.get_raw() + value.get_length());
}

static std::filesystem::path tbx_resolve_module_path(
    const tbx::PluginMeta& meta,
    tbx::IFilesystemOps& file_ops)
{
    std::filesystem::path module = meta.module_path;

    if (module.empty())
    {
        module = meta.root_directory;
    }

    if (!module.empty() && file_ops.is_directory(module))
    {
        module /= meta.name;
    }

    if (module.extension().empty())
    {
#if defined(TBX_PLATFORM_WINDOWS)
        module += ".dll";
#elif defined(TBX_PLATFORM_MACOS)
        if (module.filename().string().rfind("lib", 0) != 0)
        {
            module = module.parent_path() / (std::string("lib") + module.filename().string());
        }
        module += ".dylib";
#else
        if (module.filename().string().rfind("lib", 0) != 0)
        {
            module = module.parent_path() / (std::string("lib") + module.filename().string());
        }
        module += ".so";
#endif
    }

    return module;
}

static std::optional<tbx::LoadedPlugin> tbx_load_plugin_internal(
    const tbx::PluginMeta& meta,
    tbx::IFilesystemOps& file_ops)
{
    tbx::LoadedPlugin loaded;
    loaded.meta = meta;

    if (meta.linkage == tbx::PluginLinkage::Static)
    {
        tbx::Plugin* plug = tbx::PluginRegistry::get_instance().find_plugin(meta.name);
        if (!plug)
        {
            TBX_TRACE_WARNING("Static plugin not registered: {}", meta.name.c_str());
            return std::nullopt;
        }

        tbx::PluginInstance instance(
            plug,
            [](tbx::Plugin*)
            {
            });
        loaded.instance = std::move(instance);
        return loaded;
    }

    const std::filesystem::path module_path = tbx_resolve_module_path(meta, file_ops);
    tbx::Scope<tbx::SharedLibrary> lib(module_path);

    const std::string create_symbol = "create_" + meta.name;
    tbx::CreatePluginFn create = lib->get_symbol<tbx::CreatePluginFn>(create_symbol.c_str());
    if (!create)
    {
        TBX_TRACE_WARNING(
            "Entry point not found in plugin module: {}",
            create_symbol.c_str());
        return std::nullopt;
    }

    const std::string destroy_symbol = "destroy_" + meta.name;
    tbx::DestroyPluginFn destroy = lib->get_symbol<tbx::DestroyPluginFn>(destroy_symbol.c_str());
    if (!destroy)
    {
        TBX_TRACE_WARNING(
            "Destroy entry point not found in plugin module: {}",
            destroy_symbol.c_str());
        return std::nullopt;
    }

    tbx::Plugin* plugin_instance = create();
    if (!plugin_instance)
    {
        TBX_TRACE_WARNING("Plugin factory returned null for: {}", meta.name.c_str());
        return std::nullopt;
    }

    tbx::PluginInstance instance(plugin_instance, destroy);
    loaded.instance = std::move(instance);
    loaded.library = std::move(lib);
    return loaded;
}

namespace tbx
{

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids,
        IFilesystemOps& file_ops)
    {
        std::vector<LoadedPlugin> loaded;

        std::vector<PluginMeta> discovered;
        if (file_ops.exists(directory))
        {
            for (const auto& entry : file_ops.recursive_directory_entries(directory))
            {
                if (!entry.is_regular_file)
                {
                    continue;
                }

                auto p = entry.path;
                const std::string name = p.filename().string();
                const tbx::String lowered_string = tbx::get_lower_case(name.c_str());
                const std::string lowered_name = tbx_copy_string(lowered_string);
                if (p.extension() == ".meta" || lowered_name == "plugin.meta")
                {
                    try
                    {
                        std::string manifest_data;
                        if (!file_ops.read_text_file(p, manifest_data))
                        {
                            continue;
                        }

                        PluginMeta m = parse_plugin_meta(manifest_data, p);
                        discovered.push_back(std::move(m));
                    }
                    catch (...)
                    {
                        TBX_TRACE_WARNING("Plugin {} is unable to be loaded!", p.string().c_str());
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
            std::unordered_map<std::string, size_t> by_name_lookup;
            std::unordered_map<std::string, std::vector<size_t>> by_type;
            by_name_lookup.reserve(discovered.size());
            by_type.reserve(discovered.size());
            for (size_t index = 0; index < discovered.size(); ++index)
            {
                const PluginMeta& meta = discovered[index];
                const tbx::String lowered_name_string = tbx::get_lower_case(meta.name.c_str());
                const std::string lowered_name = tbx_copy_string(lowered_name_string);
                by_name_lookup.emplace(lowered_name, index);
                if (!meta.type.empty())
                {
                    const tbx::String lowered_type_string = tbx::get_lower_case(meta.type.c_str());
                    const std::string lowered_type = tbx_copy_string(lowered_type_string);
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
                const tbx::String trimmed_string = tbx::get_trimmed(token.c_str());
                const std::string trimmed = tbx_copy_string(trimmed_string);
                const tbx::String lowered = tbx::get_lower_case(trimmed.c_str());
                std::string needle = tbx_copy_string(lowered);
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
            if (auto instance = tbx_load_plugin_internal(meta, file_ops))
            {
                loaded.push_back(std::move(*instance));
            }
        }
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::filesystem::path& directory,
        const std::vector<std::string>& requested_ids)
    {
        return load_plugins(directory, requested_ids, get_default_filesystem_ops());
    }

    std::vector<LoadedPlugin> load_plugins(
        const std::vector<PluginMeta>& metas,
        IFilesystemOps& file_ops)
    {
        std::vector<LoadedPlugin> loaded;
        for (const PluginMeta& meta : metas)
        {
            if (auto instance = tbx_load_plugin_internal(meta, file_ops))
            {
                loaded.push_back(std::move(*instance));
            }
        }
        return loaded;
    }

    std::vector<LoadedPlugin> load_plugins(const std::vector<PluginMeta>& metas)
    {
        return load_plugins(metas, get_default_filesystem_ops());
    }
}
