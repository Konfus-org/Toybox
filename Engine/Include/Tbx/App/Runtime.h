#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Collections/LayerStack.h"
#include "Tbx/Time/DeltaTime.h"

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
        void FixedUpdate(const DeltaTime& deltaTime);
        void Update(const DeltaTime& deltaTime);
        void LateUpdate(const DeltaTime& deltaTime);
        void Shutdown();

    protected:
        /// <summary>
        /// Called when the owning app is started.
        /// </summary>
        virtual void OnStart() {}

        /// <summary>
        /// Called before the main update loop. Intended for fixed timestep logic.
        /// <param name="deltaTime">The time step used for this update.</param>
        /// </summary>
        virtual void OnFixedUpdate(const DeltaTime&) {}

        /// <summary>
        /// Called when the owning app is updated.
        /// <param name="deltaTime">The elapsed time since the previous frame.</param>
        /// </summary>
        virtual void OnUpdate(const DeltaTime&) {}

        /// <summary>
        /// Called after the main update loop. Intended for late frame work.
        /// <param name="deltaTime">The elapsed time since the previous frame.</param>
        /// </summary>
        virtual void OnLateUpdate(const DeltaTime&) {}

        /// <summary>
        /// Called when the owning app is shutting down.
        /// </summary>
        virtual void OnShutdown() {}

    protected:
        Ref<AssetServer> Assets = nullptr;
        EventCarrier Carrier = {};
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
