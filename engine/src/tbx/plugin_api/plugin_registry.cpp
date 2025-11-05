#include "tbx/plugin_api/plugin_registry.h"
#include "tbx/strings/string_utils.h"

namespace tbx
{
    PluginRegistry& PluginRegistry::get_instance()
    {
        static PluginRegistry registry;
        return registry;
    }

    void PluginRegistry::register_plugin(Plugin* plugin)
    {
        _plugins.push_back(plugin);
    }

    void PluginRegistry::unregister_plugin(Plugin* plugin)
    {
        auto it = std::remove(_plugins.begin(), _plugins.end(), plugin);
        _plugins.erase(it, _plugins.end());
    }

    std::vector<Plugin*> PluginRegistry::get_registered_plugins() const
    {
        return _plugins;
    }
}
