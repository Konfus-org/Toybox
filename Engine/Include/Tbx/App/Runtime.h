#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Collections/LayerStack.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/App/App.h"

namespace Tbx
{
    /// <summary>
    /// A way to add logic that hooks into an apps lifetime.
    /// An example use case is game specific logic.
    /// </summary>
    class IRuntime
    {
    public:
        virtual ~IRuntime() {}

        /// <summary>
        /// Called when the owning app is started.
        /// </summary>
        virtual void OnStart(App* owningApp) {}

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
        LayerStack Layers = {};
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
