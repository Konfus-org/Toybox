#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/std/string.h"
#include <algorithm>

static std::string tbx_copy_string(const tbx::String& value)
{
    return std::string(value.get_raw(), value.get_raw() + value.get_length());
}

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
            const tbx::String lowered_string = tbx::get_lower_case(name.c_str());
            const std::string lowered = tbx_copy_string(lowered_string);
            _plugins_by_name[lowered] = plugin;
        }
    }

    void PluginRegistry::unregister_plugin(const std::string& name)
    {
        const tbx::String lowered_string = tbx::get_lower_case(name.c_str());
        const std::string lowered = tbx_copy_string(lowered_string);
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

        const tbx::String lowered_string = tbx::get_lower_case(name.c_str());
        const std::string key = tbx_copy_string(lowered_string);
        const auto it = _plugins_by_name.find(key);
        if (it == _plugins_by_name.end())
        {
            return nullptr;
        }

        return it->second;
    }
}
