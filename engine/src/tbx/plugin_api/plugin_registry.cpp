#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/strings/string_utils.h"
#include <algorithm>

namespace tbx
{
    PluginRegistry& PluginRegistry::instance()
    {
        static PluginRegistry registry;
        return registry;
    }

    void PluginRegistry::register_plugin(const std::string& name, Plugin* plugin)
    {
        if (!plugin)
        {
            return;
        }
        if (std::find(_plugins.begin(), _plugins.end(), plugin) == _plugins.end())
        {
            _plugins.push_back(plugin);
        }
        if (!name.empty())
        {
            _plugins_by_name[to_lower_case_string(name)] = plugin;
        }
    }

    void PluginRegistry::unregister_plugin(const std::string& name, Plugin* plugin)
    {
        if (!plugin)
        {
            return;
        }

        if (!name.empty())
        {
            std::string key = to_lower_case_string(name);
            auto map_it = _plugins_by_name.find(key);
            if (map_it != _plugins_by_name.end() && map_it->second == plugin)
            {
                _plugins_by_name.erase(map_it);
            }
        }

        unregister_plugin(plugin);
    }

    void PluginRegistry::unregister_plugin(Plugin* plugin)
    {
        if (!plugin)
        {
            return;
        }

        auto it = std::remove(_plugins.begin(), _plugins.end(), plugin);
        _plugins.erase(it, _plugins.end());

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

        std::string key = to_lower_case_string(name);
        auto it = _plugins_by_name.find(key);
        if (it == _plugins_by_name.end())
        {
            return nullptr;
        }

        return it->second;
    }
}
