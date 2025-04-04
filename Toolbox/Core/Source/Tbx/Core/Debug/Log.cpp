#include "Tbx/Core/PCH.h"
#include "Tbx/Core/Debug/Log.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/Events/EventDispatcher.h"
#include "Tbx/Core/Events/LogEvents.h"

namespace Tbx
{
    static const std::string _logName = "Tbx::Core";
    bool Log::_isOpen = false;

    static void WriteToConsole(const std::string& msg, LogLevel lvl)
    {
        auto event = WriteLineToLogRequestEvent(lvl, msg, "Tbx::Core");
        if (!EventDispatcher::Dispatch(event)) WriteToConsole(msg, lvl);
    }

    void WriteToLogEventDispatcher::Dispatch(const std::string& msg, const LogLevel& lvl) const
    {
        // Open log if not already open
        if (!Log::IsOpen())
        {
            Log::Open();
        }

        // Send message
        auto event = WriteLineToLogRequestEvent(lvl, msg, "Tbx::Core");
        if (!EventDispatcher::Dispatch(event)) WriteToConsole(msg, lvl);
    }

    void Log::Open()
    {
        _isOpen = true;

#ifdef TBX_DEBUG
        // No log file in debug
        auto event = OpenLogRequestEvent("", _logName);
#else 
        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto& logPath = std::format("Logs\\{}.log", currentTime);
        auto event = OpenLogRequestEvent(logPath, _logName);
#endif

        EventDispatcher::Dispatch(event);
        TBX_ASSERT(event.IsHandled, "Failed to open the log {}, is a logger created and listening?", _logName);
    }

    bool Log::IsOpen()
    {
        return _isOpen;
    }

    void Log::Close()
    {
        auto event = CloseLogRequestEvent(_logName);

        EventDispatcher::Dispatch(event);
        TBX_ASSERT(event.IsHandled, "Failed to close the log {}, is a logger under that name created and listening?", _logName);

        _isOpen = false;
    }
}