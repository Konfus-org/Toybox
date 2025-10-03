#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    Runtime::Runtime(
        const std::string& name,
        Ref<EventBus> eventBus)
        : Plugin(std::move(eventBus))
        , _name(name)
    {
    }

    void Runtime::Initialize(Ref<AssetServer> assetServer)
    {
        _assetServer = std::move(assetServer);
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
        TBX_ASSERT(_assetServer != nullptr, "Runtime: Asset server is not available");
        return *_assetServer;
    }

    EventBus& Runtime::GetEventBus() const
    {
        auto eventBus = Plugin::GetEventBus();
        TBX_ASSERT(eventBus != nullptr, "Runtime: Event bus is not available");
        return *eventBus;
    }
}