#pragma once
#include "ToolboxAPI.h"
#include "LogLevel.h"
#include "Event Dispatcher/Event.h"
#include <format>

namespace Tbx
{
    class TBX_API LogEvent : public Event
    {
    public:
        int GetCategorization() const final
        {
            return static_cast<int>(EventCategory::Debug);
        }
    };

    class OpenLogRequestEvent : public LogEvent
    {
    public:
        // Don't open file, just create logger and write to std::out
        explicit TBX_API OpenLogRequestEvent(const std::string& logName)
            : _logName(logName) {}

        // Open log file and write to it as well as std::out
        TBX_API OpenLogRequestEvent(const std::string& logFilePath, const std::string& logName) 
            : _logFilePath(logFilePath), _logName(logName) {}

        TBX_API std::string ToString() const override
        {
            return "OpenLogEvent";
        }

        TBX_API std::string GetLogFilePath() const { return _logFilePath; }
        TBX_API std::string GetLogName() const { return _logName; }

    private:
        std::string _logFilePath = "";
        std::string _logName = "";
    };

    class CloseLogRequestEvent : public LogEvent
    {
    public:
        explicit TBX_API CloseLogRequestEvent(const std::string& logName)
            : _logName(logName) { }

        TBX_API std::string ToString() const override
        {
            return "OpenLogEvent";
        }

        TBX_API std::string GetNameOfLogToClose() const { return _logName; }

    private:
        std::string _logName = "";
    };

    class WriteLineToLogRequestEvent : public LogEvent
    {
    public:
        TBX_API WriteLineToLogRequestEvent(const LogLevel& level, const std::string& lineToWrite, const std::string& logToWriteTo)
            : _level(level), _lineToWrite(lineToWrite), _logToWriteTo(logToWriteTo){}

        TBX_API std::string ToString() const override
        {
            return "WriteToLogEvent";
        }

        TBX_API LogLevel GetLogLevel() const { return _level; }
        TBX_API std::string GetLineToWriteToLog() const { return _lineToWrite; }
        TBX_API std::string GetNameOfLogToWriteTo() const { return _logToWriteTo; }

    private:
        std::string _logToWriteTo = "";
        std::string _lineToWrite = "";
        LogLevel _level = LogLevel::Trace;
    };
}