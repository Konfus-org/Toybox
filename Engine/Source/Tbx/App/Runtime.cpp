#include "Tbx/PCH.h"
#include "Tbx/App/Runtime.h"

namespace Tbx
{
    Runtime::Runtime(const std::string& name)
        : Layer(name)
    {
    }

    void Runtime::Initialize()
    {
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
}