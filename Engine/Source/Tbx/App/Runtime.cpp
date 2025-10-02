#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    Runtime::Runtime(
        const std::string& name,
        Ref<AssetServer> assetServer,
        Ref<EventBus> eventBus)
        : _name(name)
        , _assetServer(assetServer)
        , _eventBus(eventBus)
    {
    }

    void Runtime::Initialize()
    {
        // Hook for inheriting runtimes
        OnStart();
    }

    void Runtime::Update()
    {
        // Hook for inheriting runtimes
        OnUpdate();
    }

    void Runtime::Shutdown()
    {
        // Hook for inheriting runtimes
        OnShutdown();
    }

    AssetServer& Runtime::GetAssetServer() const
    {
        return *_assetServer;
    }

    EventBus& Runtime::GetEventBus() const
    {
        return *_eventBus;
    }
}