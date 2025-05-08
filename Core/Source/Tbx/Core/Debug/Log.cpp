#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Debug/Log.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/Events/EventCoordinator.h"
#include "Tbx/Core/Events/LogEvents.h"
#include <iostream>

namespace Tbx
{
    static const std::string _logName = "Tbx::Core";

    std::string Log::_logFilePath = "";
    bool Log::_isOpen = false;

    static void WriteToConsole(const std::string& msg, LogLevel lvl)
    {
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

    void WriteToLogEventDispatcher::Dispatch(const std::string& msg, const LogLevel& lvl) const
    {
        // Open log if not already open
        if (!Log::IsOpen())
        {
            Log::Open();
        }

        // Send message
        auto event = WriteLineToLogRequest(lvl, msg, _logName, Log::GetFilePath());
        if (!EventCoordinator::Send(event)) WriteToConsole(msg, lvl);
    }

    void Log::Open()
    {
        _isOpen = true;

#ifdef TBX_DEBUG
        // No log file in debug
        auto event = OpenLogRequest("", _logName);
#else 
        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        _logFilePath = std::format("Logs\\{}.log", currentTime);
        auto event = OpenLogRequestEvent(_logFilePath, _logName);
#endif

        EventCoordinator::Send(event);
        TBX_ASSERT(event.IsHandled, "Failed to open the log {}, is a logger created and listening?", _logName);
    }

    bool Log::IsOpen()
    {
        return _isOpen;
    }

    void Log::Close()
    {
        auto event = CloseLogRequest(_logName);

        EventCoordinator::Send(event);
        TBX_ASSERT(event.IsHandled, "Failed to close the log {}, is a logger under that name created and listening?", _logName);

        _isOpen = false;
    }

    std::string Log::GetFilePath()
    {
        return _logFilePath;
    }
}