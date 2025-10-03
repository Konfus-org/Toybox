#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT Plugin
    {
    public:
        Plugin(Ref<EventBus> eventBus);
        virtual ~Plugin();

        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;

        bool IsBound() const;
        void Bind(const PluginMeta& pluginInfo, ExclusiveRef<SharedLibrary> library, WeakRef<Plugin> self);

        const PluginMeta& GetMeta() const;
        const SharedLibrary* GetLibrary() const;
        void ListSymbols() const;
        Ref<EventBus> GetEventBus() const;

    private:
        PluginMeta _pluginInfo = {};
        ExclusiveRef<SharedLibrary> _library = nullptr;
        bool _isBound = false;
        Ref<EventBus> _eventBus = nullptr;
    };

    class TBX_EXPORT StaticPlugin
    {
    public:
        StaticPlugin(const PluginMeta& pluginInfo, Ref<EventBus> eventBus);
        virtual ~StaticPlugin();

        StaticPlugin(const StaticPlugin&) = delete;
        StaticPlugin& operator=(const StaticPlugin&) = delete;
        StaticPlugin(StaticPlugin&&) = delete;
        StaticPlugin& operator=(StaticPlugin&&) = delete;

        const PluginMeta& GetMeta() const;

        Ref<EventBus> GetEventBus() const;

    private:
        PluginMeta _pluginInfo = {};
        Ref<EventBus> _eventBus = nullptr;
    };

    using PluginLoadFn = Plugin* (*)(
        Ref<EventBus> eventBus);
    using PluginUnloadFn = void (*)(Plugin* plugin);

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
            delete pluginToUnload;\
        }

}
