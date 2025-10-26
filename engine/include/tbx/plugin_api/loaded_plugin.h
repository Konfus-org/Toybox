#pragma once
#include "tbx/memory/smart_pointers.h"
#include "tbx/os/shared_library.h"
#include "tbx/plugin_api/plugin.h"
#include "tbx/plugin_api/plugin_meta.h"

namespace tbx
{
    struct LoadedPlugin
    {
        PluginMeta meta;
        Scope<SharedLibrary> library; // only set for dynamic plugins
        Scope<Plugin> instance;
    };
}
