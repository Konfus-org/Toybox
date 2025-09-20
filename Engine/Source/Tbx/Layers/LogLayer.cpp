#include "Tbx/PCH.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Debug/Log.h"
#ifndef TBX_DEBUG
#include <chrono>
#endif

namespace Tbx
{
    LogLayer::LogLayer(Ref<ILoggerFactory> loggerFactory) 
        : Layer("Logging")
    {
#ifdef TBX_DEBUG
        // No log file in debug
        _logger = loggerFactory->Create("Tbx");
#else
        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        _logFilePath = std::format("Logs\\{}.log", currentTime);
        _logger = loggerFactory->Create("Tbx", _logFilePath);
#endif
    }

    void LogLayer::OnAttach()
    {
        Log::Initialize(_logger);
    }

    void LogLayer::OnDetach()
    {
        // Then we can shutdown the log
        Log::Shutdown();
    }

    void LogLayer::OnUpdate()
    {
        Log::WriteQueued();
    }
}
