#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/PluginAPI/PluginServer.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
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
        _logger = loggerFactory->Create(name, _logFilePath);
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

    std::string Log::GetFolderPath()
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
                std::cout << "Tbx::Fallback::Trace: " << msg << std::endl;
                break;
            case Debug:
                std::cout << "Tbx::Fallback::Debug: " << msg << std::endl;
                break;
            case Info:
                std::cout << "Tbx::Fallback::Info: " << msg << std::endl;
                break;
            case Warn:
                std::cout << "Tbx::Fallback::Warn: " << msg << std::endl;
                break;
            case Error:
                std::cout << "Tbx::Fallback::Error: " << msg << std::endl;
                break;
            case Critical:
                std::cout << "Tbx::Fallback::Critical: " << msg << std::endl;
                break;
            default:
                std::cout << "Tbx::Fallback::LEVEL_NOT_DEFINED : " << msg << std::endl;
                break;
        }
    }
}