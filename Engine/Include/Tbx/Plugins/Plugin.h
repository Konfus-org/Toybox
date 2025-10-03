#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Memory/Refs.h"
#include <cstddef>

namespace Tbx
{
    using PluginLoadFn = void* (*)(
        Ref<EventBus> eventBus);
    using PluginUnloadFn = void (*)(void* plugin);

    class TBX_EXPORT Plugin
    {
    public:
        Plugin(
            const PluginMeta& pluginInfo,
            Ref<EventBus> eventBus);
        ~Plugin();

        bool IsValid() const;
        const PluginMeta& GetMeta() const;
        SharedLibrary& GetLibrary();
        size_t UseCount() const;

        template <typename T>
        Ref<T> As() const
        {
            return std::static_pointer_cast<T>(_instance);
        }

        Ref<void> Instance() const;

    private:
        void Load(Ref<EventBus> eventBus);
        void Unload();

        PluginMeta _pluginInfo = {};
        SharedLibrary _library = {};
        Ref<void> _instance = nullptr;
    };

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
