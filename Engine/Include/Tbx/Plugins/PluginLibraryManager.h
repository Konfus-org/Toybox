#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class SharedLibrary;
    struct PluginDestroyedEvent;

    class TBX_EXPORT PluginLibraryManager
    {
    public:
        PluginLibraryManager() = delete;

        static void Register(const Ref<Plugin>& plugin, ExclusiveRef<SharedLibrary>&& library, const PluginMeta& meta);
        static void Unregister(const Plugin* plugin);
    private:
        static void EnsureInitialized();
        static void HandleDestroyed(PluginDestroyedEvent& event);
    };
}
