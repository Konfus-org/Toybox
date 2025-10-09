#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#ifndef TBX_DEBUG
#include "Tbx/Files/Paths.h"
#endif

namespace Tbx
{
    // TODO: The log should be a static plugin that can be replaced by the user if they want.

    std::queue<std::pair<LogLevel, std::string>> Log::_logQueue = {};
    Ref<ILogger> Log::_logger = nullptr;
    std::string Log::_logFilePath = "";

    void Log::SetLogger(Ref<ILogger> logger)
    {
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
    }

    void Log::Close()
    {
        Flush();
        _logger = nullptr;
    }

    void Log::Write(LogLevel lvl, const std::string& msg)
    {
        _logQueue.push({ lvl, msg });
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
            else
            {
                // TODO: just write to std::out if we aren't using a logger
            }
        }
    }

    std::string Log::GetFilePath()
    {
        return _logFilePath;
    }
}