#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/strings/string_utils.h"

namespace tbx
{
    PluginRegistry& PluginRegistry::instance()
    {
        static PluginRegistry registry;
        return registry;
    }

    void PluginRegistry::register_plugin(const std::string& entry_point, Plugin* plugin)
    {
        if (!plugin)
        {
            return;
        }

        std::scoped_lock guard(_mutex);
        _plugins.emplace(plugin, to_lower_case_string(entry_point));
    }

    void PluginRegistry::unregister_plugin(Plugin* plugin)
    {
        if (!plugin)
        {
            return;
        }

        std::scoped_lock guard(_mutex);
        _plugins.erase(plugin);
    }

    std::vector<Plugin*> PluginRegistry::get_active_plugins() const
    {
        std::scoped_lock guard(_mutex);
        std::vector<Plugin*> result;
        result.reserve(_plugins.size());
        for (const auto& [plugin, _] : _plugins)
        {
            result.push_back(plugin);
        }
        return result;
    }

    void PluginRegistry::register_static_plugin_entry(
        std::string entry_point,
        CreatePluginFn create,
        DestroyPluginFn destroy)
    {
        if (entry_point.empty())
        {
            return;
        }

        std::scoped_lock guard(_mutex);
        _static_entries[to_lower_case_string(entry_point)] = StaticPluginEntry{ create, destroy };
    }

    void PluginRegistry::unregister_static_plugin_entry(const std::string& entry_point)
    {
        if (entry_point.empty())
        {
            return;
        }

        std::scoped_lock guard(_mutex);
        _static_entries.erase(to_lower_case_string(entry_point));
    }

    std::optional<StaticPluginEntry> PluginRegistry::find_static_plugin_entry(const std::string& entry_point) const
    {
        if (entry_point.empty())
        {
            return std::nullopt;
        }

        std::scoped_lock guard(_mutex);
        auto it = _static_entries.find(to_lower_case_string(entry_point));
        if (it == _static_entries.end())
        {
            return std::nullopt;
        }
        return it->second;
    }
}
