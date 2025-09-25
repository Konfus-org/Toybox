#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    // Forward declaration for below macro
    class App;

    /// <summary>
    /// Anything in the tbx lib that is intented to be a plugin should inherit from this interface!
    /// Plugins defined inside plugin libraries can also directly inherit from this if they only need to hook into things on load/unload.
    /// Otherwise its recommended to implement one of the TBX interfaces that inherit from this directly (defined below this interface in PluginInterfaces.h).
    /// </summary>
    class TBX_EXPORT Plugin
    {
    public:
        virtual ~Plugin() = default;
        virtual void OnLoad() {}
        virtual void OnUnload() {}
    };

    // C-linkage factory function names the host will look up.
    using PluginLoadFn = Plugin*(*)(WeakRef<App> app);
    using PluginUnloadFn = void (*)(Plugin* plugin);
}

#define TBX_LOAD_PLUGIN_FN_NAME "Load"
#define TBX_UNLOAD_PLUGIN_FN_NAME "Unload"

// Cross-platform export for the *factories* only:
#if defined(TBX_PLATFORM_WINDOWS)
    #define TBX_PLUGIN_API extern "C" __declspec(dllexport)
#else
    #define TBX_PLUGIN_API extern "C" __attribute__((visibility("default")))
#endif

/// <summary>
/// Macro to register a plugin to the TBX plugin system.
/// Is required for TBX to be able to load the plugin.
/// </summary>
#define TBX_REGISTER_PLUGIN(pluginType) \
    extern "C" TBX_PLUGIN_API pluginType* Load(Tbx::WeakRef<Tbx::App> app)\
    {\
        auto plugin = new pluginType(app);\
        plugin->OnLoad();\
        return plugin;\
    }\
    extern "C" TBX_PLUGIN_API void Unload(pluginType* pluginToUnload)\
    {\
        pluginToUnload->OnUnload();\
        delete pluginToUnload;\
    }