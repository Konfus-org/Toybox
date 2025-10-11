#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    void IRuntime::Initialize(Ref<AssetServer> assetServer, Ref<EventBus> eventBus)
    {
        Assets = assetServer;
        Dispatcher = eventBus;
        Listener.Bind(eventBus);

        // Hook for inheriting runtimes
        OnStart();
    }

    void IRuntime::FixedUpdate()
    {
        // Hook for inheriting runtimes
        OnFixedUpdate();
    }

    void IRuntime::Update()
    {
        // Hook for inheriting runtimes
        OnUpdate();
    }

    void IRuntime::LateUpdate()
    {
        // Hook for inheriting runtimes
        OnLateUpdate();
    }

    void IRuntime::Shutdown()
    {
        // Hook for inheriting runtimes
        OnShutdown();
    }
}