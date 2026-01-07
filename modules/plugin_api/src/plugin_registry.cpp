#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/common/string_utils.h"
#include <algorithm>

namespace tbx
{
    PluginRegistry& PluginRegistry::get_instance()
    {
        static PluginRegistry registry;
        return registry;
    }

    void PluginRegistry::register_plugin(const std::string& name, Plugin* plugin)
    {
        if (std::ranges::find(_plugins, plugin) == _plugins.end())
        {
            _plugins.push_back(plugin);
            _plugins_by_name[ToLower(name)] = plugin;
        }
    }

    void PluginRegistry::unregister_plugin(const std::string& name)
    {
        const std::string lowered = ToLower(name);
        auto it = _plugins_by_name.find(lowered);
        if (it == _plugins_by_name.end())
        {
            return;
        }

        auto list_it = std::ranges::find(_plugins, it->second);
        if (list_it != _plugins.end())
        {
            _plugins.erase(list_it);
        }
        _plugins_by_name.erase(lowered);
    }

    void PluginRegistry::unregister_plugin(Plugin* plugin)
    {
        auto list_it = std::ranges::find(_plugins, plugin);
        if (list_it != _plugins.end())
        {
            _plugins.erase(list_it);
        }
        for (auto map_it = _plugins_by_name.begin(); map_it != _plugins_by_name.end();)
        {
            if (map_it->second == plugin)
            {
                map_it = _plugins_by_name.erase(map_it);
            }
            else
            {
                ++map_it;
            }
        }
    }

    std::vector<Plugin*> PluginRegistry::get_registered_plugins() const
    {
        return _plugins;
    }

    Plugin* PluginRegistry::find_plugin(const std::string& name) const
    {
        if (name.empty())
        {
            return nullptr;
        }

        const std::string key = ToLower(name);
        const auto it = _plugins_by_name.find(key);
        if (it == _plugins_by_name.end())
        {
            return nullptr;
        }

        return it->second;
    }
}
