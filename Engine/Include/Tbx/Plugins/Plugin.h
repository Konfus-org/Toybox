#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Plugins/PluginMeta.h"
#include "Tbx/Plugins/SharedLibrary.h"
#include "Tbx/Memory/Refs.h"

namespace Tbx
{
    class TBX_EXPORT Plugin
    {
    public:
        Plugin();
        virtual ~Plugin();
    };

    class TBX_EXPORT IProductOfPluginFactory
    {
    public:
        virtual ~IProductOfPluginFactory();

    public:
        Ref<Plugin> Owner = nullptr;
    };

    template <typename TProduct>
    requires std::is_base_of_v<IProductOfPluginFactory, TProduct>
    class FactoryPlugin : public Plugin, public std::enable_shared_from_this<FactoryPlugin<TProduct>>
    {
    public:
        FactoryPlugin() = default;

        // Create a new instance. The factory ensures plugin ownership is injected.
        template <typename... Args>
        Ref<TProduct> Create(Args&&... args)
        {
            auto product = Ref<TProduct>(new TProduct(std::forward<Args>(args)...), [&](TProduct* prodToDelete) { Destroy(prodToDelete); });
            product->Owner = this->shared_from_this();
            return product;
        }

        void Destroy(TProduct* product)
        {
            delete product;
        }
    };

    class TBX_EXPORT StaticPlugin
    {
    public:
        StaticPlugin();
        virtual ~StaticPlugin();
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
