#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/DebugAPI.h"
#include "Tbx/Core/Debug/LogLevel.h"
#include "Tbx/Core/Debug/ILoggable.h"
#include <iostream>

namespace Tbx
{
    class WriteToLogEventDispatcher
    {
    public:
        EXPORT void Dispatch(const std::string& msg, const LogLevel& lvl) const;
    };

    class Log
    {
    public:
        EXPORT static void Open();
        EXPORT static bool IsOpen();
        EXPORT static void Close();

        EXPORT static std::string GetFilePath();

        EXPORT static inline void Trace(const ILoggable& loggable)
        {
            _dispatcher.Dispatch(loggable.ToString(), LogLevel::Trace);
        }

        EXPORT static inline void Info(const ILoggable& loggable)
        {
            _dispatcher.Dispatch(loggable.ToString(), LogLevel::Info);
        }

        EXPORT static inline void Warn(const ILoggable& loggable)
        {
            _dispatcher.Dispatch(loggable.ToString(), LogLevel::Warn);
        }

        EXPORT static inline void Error(const ILoggable& loggable)
        {
            _dispatcher.Dispatch(loggable.ToString(), LogLevel::Error);
        }

        EXPORT static inline void Critical(const ILoggable& loggable)
        {
            _dispatcher.Dispatch(loggable.ToString(), LogLevel::Critical);
        }

        template<typename... Args>
        EXPORT static inline void Trace(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Trace);
        }

        template<typename... Args>
        EXPORT static inline void Info(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Info);
        }

        template<typename... Args>
        EXPORT static inline void Warn(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Warn);
        }

        template<typename... Args>
        EXPORT static inline void Error(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Error);
        }

        template<typename... Args>
        EXPORT static inline void Critical(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
        }

    private:
        static inline WriteToLogEventDispatcher _dispatcher = {};
        static std::string _logFilePath;
        static bool _isOpen;
    };
}
