#include "Tbx/PCH.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Debug/Log.h"
#ifndef TBX_DEBUG
#include <chrono>
#endif

namespace Tbx
{
    LogLayer::LogLayer(Ref<ILogger> logger)
        : Layer("Logging")
       , _logger(logger)
    {
    }

    void LogLayer::OnAttach()
    {
        Log::Initialize(_logger);
    }

    void LogLayer::OnDetach()
    {
        // Then we can shutdown the log
        //Log::Shutdown(); We don't need to call this here, as it will be called by the Application
    }

    void LogLayer::OnUpdate()
    {
        Log::Flush();
    }
}
