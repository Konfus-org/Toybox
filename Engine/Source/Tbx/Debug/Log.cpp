#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Debug/Asserts.h"
#ifndef TBX_DEBUG
#include "Tbx/Files/Paths.h"
#endif

namespace Tbx
{
    // TODO: The log should be a static plugin that can be replaced by the user if they want.

    std::queue<std::pair<LogLevel, std::string>> Log::_logQueue = {};
    Ref<ILogger> Log::_logger = nullptr;
    std::string Log::_logFilePath = "";
    bool Log::_isOpen = false;

    void Log::Initialize(Ref<ILogger> logger)
    {
        if (_isOpen)
        {
            return;
        }

        _logger = logger;

#ifdef TBX_DEBUG
        // No log file in debug
        _logger->Open("Tbx", "");
#else
        // TODO: delete old log files! Only keep the last 10 or so...
        // Open log file in non-debug
        const auto currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto logFilePath = std::format("{}/{}.log", FileSystem::GetLogsDirectory(), currentTime);
        _logger->Open("Tbx", logFilePath);
#endif
        _isOpen = true;
    }

    void Log::Shutdown()
    {
        Flush();
        _logger = nullptr;
        _isOpen = false;
    }

    bool Log::IsOpen()
    {
        return _isOpen;
    }

    void Log::Write(LogLevel lvl, const std::string& msg)
    {
        _logQueue.push({ lvl, msg });
        if (_isOpen)
        {
            // Attempt to process immediately... 
            // but if the log hasn't been opened yet for whatever reason we have to wait for either the next write or a flush call.
            Flush();
        }
    }

    void Log::Flush()
    {
        while (!_logQueue.empty())
        {
            auto queuedItem = _logQueue.front();
            auto lvl = queuedItem.first;
            auto msg = queuedItem.second;
            _logQueue.pop();

            if (_logger != nullptr)
            {
                // Use our logger
                _logger->Write((int)lvl, msg);
                continue;
            }
            else if (_isOpen)
            {
                TBX_ASSERT(false, "Log: No logger instance available to write log message!");
            }
        }
    }

    std::string Log::GetFilePath()
    {
        return _logFilePath;
    }
}