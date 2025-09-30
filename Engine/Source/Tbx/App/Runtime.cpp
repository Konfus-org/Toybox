#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    Runtime::Runtime(const std::string& name)
        : Layer(name)
    {
    }

    void Runtime::Initialize(
        Ref<AssetServer> assetServer,
        Ref<EventBus> eventBus)
    {
        _assetServer = assetServer;
        _eventBus = eventBus;

        // Hook for inheriting runtimes
        OnStart();
    }

    void Runtime::Shutdown()
    {
        // Hook for inheriting runtimes
        OnShutdown();
    }

    void Runtime::OnAttach()
    {
        // Do nothing on attach...
    }

    void Runtime::OnDetach()
    {
        Shutdown();
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