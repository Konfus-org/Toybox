#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/os/shared_library.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_meta.h"

namespace tbx
{
    struct PluginDeleter
    {
        DestroyPluginFn destroy = nullptr;

        void operator()(Plugin* plugin) const
        {
            if (!plugin)
            {
                return;
            }

            if (destroy)
            {
                destroy(plugin);
            }
            else
            {
                delete plugin;
            }
        }
    };

    using PluginInstance = Scope<Plugin, PluginDeleter>;

    struct LoadedPlugin
    {
        PluginMeta meta;
        Scope<SharedLibrary> library; // only set for dynamic plugins
        PluginInstance instance;
    };
}
