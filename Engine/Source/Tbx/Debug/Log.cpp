#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include <iostream>

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
        // Open log file in non-debug
        const auto currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto logFilePath = std::format("Logs\\{}.log", currentTime);
        _logger->Open("Tbx", logFilePath);
#endif
        _isOpen = true;
    }

    void Log::Shutdown()
    {
        ProcessQueue();
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
            // but if the log hasn't been opened yet for whatever reason we have to wait for the next update
            ProcessQueue();
        }
    }

    void Log::ProcessQueue()
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
                // Fallback to std::out
                switch (lvl)
                {
                    using enum LogLevel;
                    case Trace:
                        std::cout << "Tbx::Trace: " << msg << std::endl;
                        break;
                    case Debug:
                        std::cout << "Tbx::Debug: " << msg << std::endl;
                        break;
                    case Info:
                        std::cout << "Tbx::Info: " << msg << std::endl;
                        break;
                    case Warn:
                        std::cout << "Tbx::Warn: " << msg << std::endl;
                        break;
                    case Error:
                        std::cout << "Tbx::Error: " << msg << std::endl;
                        break;
                    case Critical:
                        std::cout << "Tbx::Critical: " << msg << std::endl;
                        break;
                    default:
                        std::cout << "Tbx::LEVEL_NOT_DEFINED : " << msg << std::endl;
                        break;
                }
            }
        }
    }

    std::string Log::GetFolderPath()
    {
        return _logFilePath;
    }
}