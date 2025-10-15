#pragma once
#include "Tbx/Debug/LogEvents.h"
#include "Tbx/DllExport.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Memory/Refs.h"
#include <string>

namespace Tbx
{
    class EventBus;
    class ILogger;

    class TBX_EXPORT LogManager
    {
    public:
        LogManager(const std::string& channel, Ref<ILogger> logger, Ref<EventBus> bus);
        ~LogManager();

        Ref<ILogger> GetLogger() const;
        void Flush();

        void Trace(const std::string& message);
        void Debug(const std::string& message);
        void Info(const std::string& message);
        void Warn(const std::string& message);
        void Error(const std::string& message);
        void Critical(const std::string& message);
        void Write(LogLevel level, const std::string& message);

        std::string GetLogFilePath() const;

    private:
        void OnLogMessageEvent(LogMessageEvent& e);
        std::string ResolveLogFilePath(const std::string& channel) const;

    private:
        EventListener _listener = {};
        Ref<ILogger> _logger = nullptr;
        std::string _channel = "";
        std::string _logFilePath = "";
    };
}
