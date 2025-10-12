#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    void IRuntime::Initialize(Ref<AssetServer> assetServer, Ref<EventBus> eventBus)
    {
        Assets = assetServer;
        EventBus = eventBus;
        Dispatcher.Bind(eventBus);
        Listener.Bind(eventBus);

        // Hook for inheriting runtimes
        OnStart();
    }

    void IRuntime::FixedUpdate(const DeltaTime& deltaTime)
    {
        // Hook for inheriting runtimes
        OnFixedUpdate(deltaTime);
    }

    void IRuntime::Update(const DeltaTime& deltaTime)
    {
        // Hook for inheriting runtimes
        OnUpdate(deltaTime);
    }

    void IRuntime::LateUpdate(const DeltaTime& deltaTime)
    {
        // Hook for inheriting runtimes
        OnLateUpdate(deltaTime);
    }

    void IRuntime::Shutdown()
    {
        // Hook for inheriting runtimes
        OnShutdown();
    }
}