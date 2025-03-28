#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/LogLevel.h"
#include "Tbx/Core/Events/Event.h"
#include <format>

namespace Tbx
{
    class EXPORT LogEvent : public Event
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
        explicit EXPORT OpenLogRequestEvent(const std::string& logName)
            : _logName(logName) {}

        // Open log file and write to it as well as std::out
        EXPORT OpenLogRequestEvent(const std::string& logFilePath, const std::string& logName) 
            : _logFilePath(logFilePath), _logName(logName) {}

        EXPORT std::string ToString() const override
        {
            return "OpenLogEvent";
        }

        EXPORT std::string GetLogFilePath() const { return _logFilePath; }
        EXPORT std::string GetLogName() const { return _logName; }

    private:
        std::string _logFilePath = "";
        std::string _logName = "";
    };

    class CloseLogRequestEvent : public LogEvent
    {
    public:
        explicit EXPORT CloseLogRequestEvent(const std::string& logName)
            : _logName(logName) { }

        EXPORT std::string ToString() const override
        {
            return "OpenLogEvent";
        }

        EXPORT std::string GetNameOfLogToClose() const { return _logName; }

    private:
        std::string _logName = "";
    };

    class WriteLineToLogRequestEvent : public LogEvent
    {
    public:
        EXPORT WriteLineToLogRequestEvent(const LogLevel& level, const std::string& lineToWrite, const std::string& logToWriteTo)
            : _level(level), _lineToWrite(lineToWrite), _logToWriteTo(logToWriteTo){}

        EXPORT std::string ToString() const override
        {
            return "WriteToLogEvent";
        }

        EXPORT LogLevel GetLogLevel() const { return _level; }
        EXPORT std::string GetLineToWriteToLog() const { return _lineToWrite; }
        EXPORT std::string GetNameOfLogToWriteTo() const { return _logToWriteTo; }

    private:
        std::string _logToWriteTo = "";
        std::string _lineToWrite = "";
        LogLevel _level = LogLevel::Trace;
    };
}