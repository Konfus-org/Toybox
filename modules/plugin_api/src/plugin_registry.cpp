#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/common/string_extensions.h"
#include <algorithm>
#include <string>

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
            _plugins_by_name[to_lower_case_string(name)] = plugin;
        }
    }

    void PluginRegistry::unregister_plugin(const std::string& name)
    {
        const std::string lowered = to_lower_case_string(name);
        auto p = _plugins_by_name.at(lowered);
        _plugins_by_name.erase(lowered);
        std::erase(_plugins, p);
    }

    void PluginRegistry::unregister_plugin(Plugin* plugin)
    {
        std::erase(_plugins, plugin);
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

        const std::string key = to_lower_case_string(name);
        const auto it = _plugins_by_name.find(key);
        if (it == _plugins_by_name.end())
        {
            return nullptr;
        }

        return it->second;
    }
}
