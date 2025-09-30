#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    /// <summary>
    /// Anything in the tbx lib that is intented to be a plugin should inherit from this interface!
    /// Plugins defined inside plugin libraries can also directly inherit from this if they only need to hook into things on load/unload.
    /// Otherwise its recommended to implement one of the TBX interfaces that inherit from this directly (defined below this interface in PluginInterfaces.h).
    /// </summary>
    class TBX_EXPORT IPlugin
    {
    public:
        virtual ~IPlugin() = default;
    };

    // C-linkage factory function names the host will look up.
    using PluginLoadFn = IPlugin*(*)(Ref<EventBus> eventBus);
    using PluginUnloadFn = void (*)(IPlugin* plugin);

    #define TBX_LOAD_PLUGIN_FN_NAME "Load"
    #define TBX_UNLOAD_PLUGIN_FN_NAME "Unload"

    // Cross-platform export for the *factories* only:
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
    #else
        #define TBX_PLUGIN_EXPORT extern "C"
    #endif

    /// <summary>
    /// Macro to register a plugin to the TBX plugin system.
    /// Is required for TBX to be able to load the plugin.
    /// Example usage:
    /// class MyPlugin : public Tbx::IPlugin { ... };
    /// TBX_REGISTER_PLUGIN(MyPlugin)
    /// </summary>
    #define TBX_REGISTER_PLUGIN(pluginType) \
        TBX_PLUGIN_EXPORT pluginType* Load(::Tbx::Ref<::Tbx::EventBus> eventBus)\
        {\
            auto plugin = new pluginType(eventBus);\
            return plugin;\
        }\
        TBX_PLUGIN_EXPORT void Unload(pluginType* pluginToUnload)\
        {\
            delete pluginToUnload;\
        }
}