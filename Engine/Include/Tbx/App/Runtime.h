#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/App/App.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/AppEvents.h"

namespace Tbx
{
    /// <summary>
    /// This is a layer added at runtime via the plugin system via a runtime loader.
    /// </summary>
    class TBX_EXPORT Runtime : public Plugin
    {
    public:
        Runtime(
            const std::string& name,
            Ref<EventBus> eventBus);

        /// <summary>
        /// Inits a runtime.
        /// Should be called after important systems are initialized so the runtime can utilize them.
        /// </summary>
        void Initialize(Ref<AssetServer> assetServer);

        /// <summary>
        /// Updates the runtime.
        /// </summary>
        void Update();

        /// <summary>
        /// Shuts down a runtime.
        /// </summary>
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

        // TODO: hide behind protected accessors
        // Ex: GetAsset<>, GetAssets<>, AddAsset, RemoveAsset, etc...
        AssetServer& GetAssetServer() const;
        // TODO: hide behind protected accessors
        // Ex: PostEvent<>, SendEvent<>, SubscribeToEvent<>, UnsubscribeFromEvent<>, etc...
        EventBus& GetEventBus() const;

    private:
        Ref<AssetServer> _assetServer = nullptr;
        std::string _name = "";
    };

    /// <summary>
    /// Register a runtime
    /// </summary>
    #define TBX_REGISTER_RUNTIME(runtimeType) \
        TBX_REGISTER_PLUGIN(runtimeType)
}
