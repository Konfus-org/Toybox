#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/Layer.h"
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
    class TBX_EXPORT Runtime : public Layer
    {
    public:
        Runtime(const std::string& name);

        /// <summary>
        /// Starts a runtime.
        /// Should be called after important systems are initialized so the runtime can utilize them.
        /// </summary>
        void Initialize(
            Ref<AssetServer> assetServer,
            Ref<EventBus> eventBus);

        /// <summary>
        /// Shuts down a runtime.
        /// </summary>
        void Shutdown();

        // This occurs BEFORE our app has been fully initialized. So we don't want to do anything here... 
        // overriding to hide it from inheritors
        void OnAttach() final;

        // Shutdown makes more sense so we will override this to hide from inheritors, it will just call on shutdown.
        void OnDetach() final;

    protected:
        // TODO: hide behind protected accessors
        // Ex: GetAsset<>, GetAssets<>, AddAsset, RemoveAsset, etc...
        AssetServer& GetAssetServer() const;
        // TODO: hide behind protected accessors
        // Ex: PostEvent<>, SendEvent<>, SubscribeToEvent<>, UnsubscribeFromEvent<>, etc...
        EventBus& GetEventBus() const;

        virtual void OnStart() {}
        virtual void OnShutdown() {}

    private:
        Ref<AssetServer> _assetServer = nullptr;
        Ref<EventBus> _eventBus = nullptr;
    };

    template <class TRuntime>
    requires std::is_base_of_v<Runtime, TRuntime>
    class RuntimeLoader : public IPlugin
    {
    public:
        RuntimeLoader(Ref<EventBus> eventBus)
            : _listener(eventBus)
        {
            auto addRuntime = [this](const AppLaunchedEvent& e)
            {
                e.GetApp().AddLayer<TRuntime>();
                _listener.Unbind();
            };
            _listener.Listen<AppLaunchedEvent>(addRuntime);
        }

    private:
        EventListener _listener = {};
    };

    /// <summary>
    /// Register a runtime
    /// </summary>
    #define TBX_REGISTER_RUNTIME(runtimeType) \
        class runtimeType##Loader : public ::Tbx::RuntimeLoader< runtimeType > \
        { \
        public: \
            explicit runtimeType##Loader(::Tbx::Ref<::Tbx::EventBus> eventBus) \
                : ::Tbx::RuntimeLoader< runtimeType >(eventBus) \
            { \
            } \
        }; \
        TBX_REGISTER_PLUGIN(runtimeType##Loader)
}
