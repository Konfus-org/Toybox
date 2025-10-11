#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Collections/LayerStack.h"

namespace Tbx
{
    /// <summary>
    /// A way to add logic that hooks into an apps lifetime.
    /// </summary>
    class IRuntime
    {
    public:
        virtual ~IRuntime() {}

        void Initialize(Ref<AssetServer> assetServer, Ref<EventBus> eventBus);
        void FixedUpdate();
        void Update();
        void LateUpdate();
        void Shutdown();

    protected:
        /// <summary>
        /// Called when the owning app is started.
        /// </summary>
        virtual void OnStart() {}

        /// <summary>
        /// Called before the main update loop. Intended for fixed timestep logic.
        /// </summary>
        virtual void OnFixedUpdate() {}

        /// <summary>
        /// Called when the owning app is updated.
        /// </summary>
        virtual void OnUpdate() {}

        /// <summary>
        /// Called after the main update loop. Intended for late frame work.
        /// </summary>
        virtual void OnLateUpdate() {}

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

    class TBX_EXPORT StaticRuntime : public StaticPlugin, public IRuntime
    {
    };

    class TBX_EXPORT Runtime : public Plugin, public IRuntime
    {
    };

    /// <summary>
    /// Register a runtime
    /// </summary>
    #define TBX_REGISTER_RUNTIME(runtimeType) \
        TBX_REGISTER_PLUGIN(runtimeType)
}
