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
    class TBX_EXPORT Runtime
    {
    public:
        Runtime(
            const std::string& name,
            Ref<AssetServer> assetServer,
            Ref<EventBus> eventBus);

        /// <summary>
        /// Inits a runtime.
        /// Should be called after important systems are initialized so the runtime can utilize them.
        /// </summary>
        void Initialize();

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
        Ref<EventBus> _eventBus = nullptr;
        std::string _name = "";
    };

    class IRuntimeLoader
    {
    public:
        virtual Tbx::Ref<Runtime> Load(
            Ref<AssetServer> assetServer,
            Ref<EventBus> eventBus) = 0;
    };

    template <class TRuntime>
    requires std::is_base_of_v<Runtime, TRuntime>
    class RuntimeLoader : public IRuntimeLoader
    {
    public:
        RuntimeLoader(Ref<EventBus>) {}

        Tbx::Ref<Runtime> Load(
            Ref<AssetServer> assetServer,
            Ref<EventBus> eventBus) override
        {
            return MakeRef<Runtime>(assetServer, eventBus);
        }
    };

    /// <summary>
    /// Register a runtime
    /// </summary>
    #define TBX_REGISTER_RUNTIME(runtimeType) \
        class runtimeType##Loader : public ::Tbx::RuntimeLoader< runtimeType > \
        { \
        public: \
            explicit runtimeType##Loader(\
                ::Tbx::Ref<::Tbx::EventBus> eventBus) \
                : ::Tbx::RuntimeLoader< runtimeType >(eventBus) \
            { \
            } \
        }; \
        TBX_REGISTER_PLUGIN(runtimeType##Loader)
}
