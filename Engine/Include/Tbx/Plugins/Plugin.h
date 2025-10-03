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
        Plugin() = default;
        virtual ~Plugin();

        Plugin(const Plugin&) = delete;
        Plugin& operator=(const Plugin&) = delete;
        Plugin(Plugin&&) = delete;
        Plugin& operator=(Plugin&&) = delete;

        bool IsInitialized() const;

        const PluginMeta& GetMeta() const;
        SharedLibrary& GetLibrary();
        const SharedLibrary& GetLibrary() const;

        void Bind(const Ref<Plugin>& self);
        void Initialize(const PluginMeta& pluginInfo, SharedLibrary library, Ref<EventBus> eventBus);

    protected:
        Ref<EventBus> GetEventBus() const;

    private:
        void NotifyLoaded() const;
        void NotifyUnloaded() const;

        PluginMeta _pluginInfo = {};
        SharedLibrary _library = {};
        bool _isInitialized = false;
        Ref<EventBus> _eventBus = nullptr;
        WeakRef<Plugin> _self = {};
    };

    class TBX_EXPORT StaticPlugin
    {
    public:
        StaticPlugin() = default;
        virtual ~StaticPlugin();

        StaticPlugin(const StaticPlugin&) = delete;
        StaticPlugin& operator=(const StaticPlugin&) = delete;
        StaticPlugin(StaticPlugin&&) = delete;
        StaticPlugin& operator=(StaticPlugin&&) = delete;

        bool IsInitialized() const;

        const PluginMeta& GetMeta() const;

        void Initialize(const PluginMeta& pluginInfo, Ref<EventBus> eventBus);

    protected:
        Ref<EventBus> GetEventBus() const;

    private:
        void NotifyLoaded() const;
        void NotifyUnloaded() const;

        PluginMeta _pluginInfo = {};
        Ref<EventBus> _eventBus = nullptr;
        bool _isInitialized = false;
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
