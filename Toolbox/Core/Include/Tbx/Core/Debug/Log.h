#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/LogLevel.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include "Tbx/Core/Events/LogEvents.h"
#include "Tbx/Core/Events/EventDispatcher.h"
#include <iostream>

namespace Tbx
{
    class Log
    {
    public:
        EXPORT static void Open(const std::string& logSaveLocation = "");
        EXPORT static void Close();

        EXPORT static inline void Trace(const ILoggable& loggable)
        {
            SendWriteToLogEvent(loggable.ToString(), LogLevel::Trace);
        }

        EXPORT static inline void Info(const ILoggable& loggable)
        {
            SendWriteToLogEvent(loggable.ToString(), LogLevel::Info);
        }

        EXPORT static inline void Warn(const ILoggable& loggable)
        {
            SendWriteToLogEvent(loggable.ToString(), LogLevel::Warn);
        }

        EXPORT static inline void Error(const ILoggable& loggable)
        {
            SendWriteToLogEvent(loggable.ToString(), LogLevel::Error);
        }

        EXPORT static inline void Critical(const ILoggable& loggable)
        {
            SendWriteToLogEvent(loggable.ToString(), LogLevel::Critical);
        }

        template<typename... Args>
        EXPORT static inline void Trace(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Trace);
        }

        template<typename... Args>
        EXPORT static inline void Info(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Info);
        }

        template<typename... Args>
        EXPORT static inline void Warn(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Warn);
        }

        template<typename... Args>
        EXPORT static inline void Error(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Error);
        }

        template<typename... Args>
        EXPORT static inline void Critical(const std::string& fmt_str, Args&&... args)
        {
            SendWriteToLogEvent(std::vformat(fmt_str, std::make_format_args(args...)), LogLevel::Critical);
        }

    private:
        static inline const std::string _logName = "TBX";

        static inline void SendWriteToLogEvent(const std::string& msg, const LogLevel& lvl)
        {
            auto event = WriteLineToLogRequestEvent(lvl, msg, _logName);
            if (!EventDispatcher::Send(event)) FallbackLog(msg, lvl);
        }

        static inline void FallbackLog(const std::string& msg, LogLevel lvl)
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
    };
}
