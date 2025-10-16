#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT Plugin
    {
    public:
        Plugin() = default;
        virtual ~Plugin() = default;

        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;

        PluginMeta Meta = {};
        ExclusiveRef<SharedLibrary> Library = nullptr;
    };

    class TBX_EXPORT StaticPlugin
    {
    public:
        StaticPlugin() = default;
        virtual ~StaticPlugin() = default;
        PluginMeta PluginInfo = {};
    };

    using PluginLoadFn = Plugin* (*)(Ref<EventBus> eventBus);
    using PluginUnloadFn = void (*)(Plugin* plugin);

    #define TBX_LOAD_PLUGIN_FN_NAME "Load"
    #define TBX_UNLOAD_PLUGIN_FN_NAME "Unload"

    // Cross-platform export for the *factories* only:
    #if defined(TBX_PLATFORM_WINDOWS)
        #define TBX_PLUGIN_EXPORT extern "C" __declspec(dllexport)
    #else
        #define TBX_PLUGIN_EXPORT extern "C"
    #endif

    static void Delete(Plugin* plugin)
    {
        auto library = std::move(plugin->Library);
    }

    /// <summary>
    /// Macro to register a plugin to the TBX plugin system.
    /// Is required for TBX to be able to load the plugin.
    /// Example usage:
    /// class MyPlugin { ... };
    /// TBX_REGISTER_PLUGIN(MyPlugin)
    /// </summary>
    #define TBX_REGISTER_PLUGIN(pluginType) \
        TBX_PLUGIN_EXPORT pluginType* Load(\
            ::Tbx::Ref<::Tbx::EventBus> eventBus)\
        {\
            auto plugin = new pluginType(eventBus);\
            return plugin;\
        }\
        TBX_PLUGIN_EXPORT void Unload(pluginType* pluginToUnload)\
        {\
            TBX_TRACE_INFO("Plugin: Unloaded {}\n", pluginToUnload->Meta.Name);\
            ::Tbx::EventBus::Global->QueueEvent(\
                ::Tbx::MakeExclusive<::Tbx::Event>(::Tbx::PluginUnloadedEvent(pluginToUnload)));\
            auto library = std::move(pluginToUnload->Library);\
            delete pluginToUnload;\
        }
}
