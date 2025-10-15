#include "Tbx/PCH.h"
#include "Tbx/Debug/LogManager.h"
#include "Tbx/Debug/Asserts.h"
#include "Tbx/Debug/Build.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Debug/Tracers.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Files/Paths.h"
#include "Tbx/Time/CurrentTime.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <utility>

namespace Tbx
{
    LogMessageEvent::LogMessageEvent(LogLevel level, const std::string& message)
        : _level(level)
        , _message(message)
    {
    }

    LogLevel LogMessageEvent::GetLevel() const
    {
        return _level;
    }

    const std::string& LogMessageEvent::GetMessage() const
    {
        return _message;
    }

    std::string LogMessageEvent::TakeMessage()
    {
        return std::move(_message);
    }

    std::string LogMessageEvent::ToString() const
    {
        return "Log Message Event";
    }

    LogManager::LogManager(const std::string& channel, Ref<ILogger> logger, Ref<EventBus> bus)
    {
        TBX_ASSERT(bus != nullptr, "LogManager: Requires a valid event bus.\n");
        TBX_ASSERT(logger != nullptr, "LogManager: Requires a valid logger.\n");

        _logger = logger;

        _listener.Bind(bus);
        _listener.Listen<LogMessageEvent>([this](LogMessageEvent& event)
        {
            OnLogMessageEvent(event);
        });

        _channel = channel;
        _logFilePath = ResolveLogFilePath(channel);

        if (_logger)
        {
            _logger->Open(_channel, _logFilePath);
        }
        else
        {
            std::printf("Toybox: No logger available to open channel %s.\n", channel.c_str());
        }
    }

    LogManager::~LogManager()
    {
        const auto logger = _logger;
        if (logger)
        {
            logger->Flush();
            logger->Close();
        }

        _logger = nullptr;
        _logFilePath.clear();
        _channel.clear();
    }

    Ref<ILogger> LogManager::GetLogger() const
    {
        return _logger;
    }

    void LogManager::Flush()
    {
        const auto logger = _logger;
        TBX_ASSERT(logger != nullptr, "LogManager: Missing logger during flush.\n");

        if (!logger)
        {
            return;
        }

        logger->Flush();
    }

    void LogManager::Trace(const std::string& message)
    {
        Write(LogLevel::Trace, message);
    }

    void LogManager::Debug(const std::string& message)
    {
        Write(LogLevel::Debug, message);
    }

    void LogManager::Info(const std::string& message)
    {
        Write(LogLevel::Info, message);
    }

    void LogManager::Warn(const std::string& message)
    {
        Write(LogLevel::Warn, message);
    }

    void LogManager::Error(const std::string& message)
    {
        Write(LogLevel::Error, message);
    }

    void LogManager::Critical(const std::string& message)
    {
        Write(LogLevel::Critical, message);
    }

    void LogManager::Write(LogLevel level, const std::string& message)
    {
        const auto logger = _logger;
        TBX_ASSERT(logger != nullptr, "LogManager: Missing logger during write.\n");

        if (!logger)
        {
            std::printf("Toybox: Dropping log [%d]: %s\n", static_cast<int>(level), message.c_str());
            return;
        }

        logger->Write(static_cast<int>(level), message);
    }

    std::string LogManager::GetLogFilePath() const
    {
        return _logFilePath;
    }

    void LogManager::OnLogMessageEvent(LogMessageEvent& e)
    {
        Write(e.GetLevel(), e.TakeMessage());
        e.IsHandled = true;
    }

    std::string LogManager::ResolveLogFilePath(const std::string& channel) const
    {
        if (IsDebugBuild)
        {
            return "";
        }

        const auto directory = std::filesystem::path(FileSystem::GetLogsDirectory());
        std::error_code directoryError;
        std::filesystem::create_directories(directory, directoryError);
        if (directoryError)
        {
            TBX_TRACE_WARNING("LogManager: Failed to create log directory {}: {}", directory.string(), directoryError.message());
            return "";
        }

        auto sanitizedChannel = channel;
        sanitizedChannel.erase(std::remove_if(sanitizedChannel.begin(), sanitizedChannel.end(), [](unsigned char ch)
        {
            return std::isalnum(static_cast<int>(ch)) == 0 && ch != '-' && ch != '_';
        }), sanitizedChannel.end());

        if (sanitizedChannel.empty())
        {
            sanitizedChannel = "Toybox";
        }

        const auto timestamp = GetCurrentTimestamp("%Y%m%d-%H%M%S");
        const auto resolvedPath = (directory / (timestamp + "-" + sanitizedChannel + ".log")).generic_string();
        return resolvedPath;
    }
}
