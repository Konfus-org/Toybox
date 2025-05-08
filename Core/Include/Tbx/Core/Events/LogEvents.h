#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/LogLevel.h"
#include "Tbx/Core/Events/Event.h"

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

    class OpenLogRequest : public LogEvent
    {
    public:
        // Don't open file, just create logger and write to std::out
        explicit EXPORT OpenLogRequest(const std::string& logName)
            : _logName(logName) {}

        // Open log file and write to it as well as std::out
        EXPORT OpenLogRequest(const std::string& logFilePath, const std::string& logName) 
            : _logFilePath(logFilePath), _logName(logName) {}

        EXPORT std::string ToString() const final
        {
            return "Open Log Event";
        }

        EXPORT std::string GetLogFilePath() const { return _logFilePath; }
        EXPORT std::string GetLogName() const { return _logName; }

    private:
        std::string _logFilePath = "";
        std::string _logName = "";
    };

    class CloseLogRequest : public LogEvent
    {
    public:
        explicit EXPORT CloseLogRequest(const std::string& logName)
            : _logName(logName) { }

        EXPORT std::string ToString() const final
        {
            return "Close Log Event";
        }

        EXPORT std::string GetNameOfLogToClose() const { return _logName; }

    private:
        std::string _logName = "";
    };

    class WriteLineToLogRequest : public LogEvent
    {
    public:
        EXPORT WriteLineToLogRequest(const LogLevel& level, const std::string& lineToWrite, const std::string& logToWriteTo, const std::string& logFilePath)
            : _level(level), _lineToWrite(lineToWrite), _logToWriteTo(logToWriteTo), _logFilePath(logFilePath) {}

        EXPORT std::string ToString() const final
        {
            return "Write To Log Event";
        }

        EXPORT LogLevel GetLogLevel() const { return _level; }
        EXPORT std::string GetLineToWriteToLog() const { return _lineToWrite; }
        EXPORT std::string GetLogName() const { return _logToWriteTo; }
        EXPORT std::string GetLogFilePath() const { return _logFilePath; }

    private:
        LogLevel _level = LogLevel::Trace;
        std::string _lineToWrite = "";
        std::string _logToWriteTo = "";
        std::string _logFilePath = "";
    };
}