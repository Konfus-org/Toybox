#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class SharedLibrary;
    struct PluginDestroyedEvent;

    class TBX_EXPORT PluginManager
    {
    public:
        static void Register(const Ref<Plugin>& plugin, const PluginMeta& meta, ExclusiveRef<SharedLibrary>& library);
        static void Unregister(const Plugin* plugin);

    private:
        static void EnsureInitialized();
        static void HandleDestroyed(PluginDestroyedEvent& event);
    };
}
