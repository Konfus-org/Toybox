#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Plugin API/PluginServer.h"
#include "Tbx/Plugin API/PluginInterfaces.h"
#include "Tbx/Events/EventCoordinator.h"
#include <chrono>
#include <iostream>

namespace Tbx
{
    std::shared_ptr<ILogger> Log::_logger = nullptr;
    std::string Log::_logFilePath = "";
    bool Log::_isOpen = false;

    void Log::Open(const std::string& name)
    {
        _isOpen = true;

        auto loggerFactory = PluginServer::GetPlugin<ILoggerFactoryPlugin>();
        TBX_ASSERT(loggerFactory, "Logger factory plugin not found! Falling back to default console logging.");

#ifdef TBX_DEBUG
        // No log file in debug
        _logger = loggerFactory->Create(name);
#else
        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        _logFilePath = std::format("Logs\\{}.log", currentTime);
        _logger = loggerFactory->Create(name, _logFilePath);
#endif

        TBX_ASSERT(_logger, "Failed to open the log {}!", name);
    }

    bool Log::IsOpen()
    {
        return _isOpen;
    }

    void Log::Close()
    {
        _logger = nullptr;
        _isOpen = false;
    }

    std::string Log::GetFilePath()
    {
        return _logFilePath;
    }

    void Log::Write(LogLevel lvl, const std::string& msg)
    {
        if (_logger != nullptr)
        {
            _logger->Write((int)lvl, msg);
            return;
        }

        // Fallback to std::out
        switch (lvl)
        {
            using enum LogLevel;
            case Trace:
                std::cout << "Tbx::Core::Trace: " << msg << std::endl;
                break;
            case Debug:
                std::cout << "Tbx::Core::Debug: " << msg << std::endl;
                break;
            case Info:
                std::cout << "Tbx::Core::Info: " << msg << std::endl;
                break;
            case Warn:
                std::cout << "Tbx::Core::Warn: " << msg << std::endl;
                break;
            case Error:
                std::cout << "Tbx::Core::Error: " << msg << std::endl;
                break;
            case Critical:
                std::cout << "Tbx::Core::Critical: " << msg << std::endl;
                break;
            default:
                std::cout << "Tbx::Core::LEVEL_NOT_DEFINED : " << msg << std::endl;
                break;
        }
    }
}