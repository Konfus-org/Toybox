#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Collections/LayerStack.h"

namespace Tbx
{
    class IRuntime
    {
    public:
        virtual ~IRuntime() {}

        void Initialize(Ref<AssetServer> assetServer, Ref<EventBus> eventBus);
        void Update();
        void Shutdown();

    protected:
        /// <summary>
        /// Called when the owning app is started.
        /// </summary>
        virtual void OnStart() {}

        /// <summary>
        /// Called when the owning app is updated.
        /// </summary>
        virtual void OnUpdate() {}

        /// <summary>
        /// Called when the owning app is shutting down.
        /// </summary>
        virtual void OnShutdown() {}

    protected:
        Ref<AssetServer> Assets = nullptr;
        Ref<EventBus> Dispatcher = nullptr;
        LayerStack Layers = {};
        EventListener Listener = {};
    };

    /// <summary>
    /// This is a layer added at runtime via the plugin system via a runtime loader.
    /// </summary>
    class TBX_EXPORT StaticRuntime : public StaticPlugin, public IRuntime
    {
    };

    /// <summary>
    /// This is a layer added at runtime via the plugin system via a runtime loader.
    /// </summary>
    class TBX_EXPORT Runtime : public Plugin, public IRuntime
    {
    };

    /// <summary>
    /// Register a runtime
    /// </summary>
    #define TBX_REGISTER_RUNTIME(runtimeType) \
        TBX_REGISTER_PLUGIN(runtimeType)
}
