#pragma once
#include "Tbx/Core/DllExport.h"
#include "Tbx/Core/Debug/LogLevel.h"
#include "Tbx/Core/Debug/IPrintable.h"
#include <format>

namespace Tbx
{
    class WriteToLogEventDispatcher
    {
    public:
        /// <summary>
        /// Sends a msg asking a logger to write something.
        /// Will also request a logger open if we haven't already.
        /// </summary>
        EXPORT void Dispatch(const std::string& msg, const LogLevel& lvl) const;
    };

    class Log
    {
    public:
        /// <summary>
        /// Sends a msg to open a logger.
        /// If one is created and listening it will open a log file in release or console if in debug.
        /// </summary>
        EXPORT static void Open();

        /// <summary>
        /// Returns true if we requested a logger open a file 
        /// and has a logger responded confirming it has opened a file.
        /// </summary>
        EXPORT static bool IsOpen();

        /// <summary>
        /// Returns true if we requested a logger close the file it was writing to 
        /// and has a logger responded confirming it has closed the file and shutdown.
        /// </summary>
        EXPORT static void Close();

        /// <summary>
        /// Returns the path to the log file if we are logging to a file.
        /// If not returns an empty string.
        /// </summary>
        EXPORT static std::string GetFilePath();

        /// <summary>
        /// Sends a trace level msg to the log.
        /// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
        /// This is a good method to use to add debug info to the log to track down bugs.
        /// </summary>
        template<typename... Args>
        EXPORT static void Trace(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Trace);
        }

        /// <summary>
        /// Sends an info level msg to the log.
        /// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
        /// This is a good method to use to log info to the log that we want to track during runtime. Ex: window resize, layers attached, shutdown triggered, etc...
        /// </summary>
        template<typename... Args>
        EXPORT static void Info(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Info);
        }

        /// <summary>
        /// Sends a warn level msg to the log.
        /// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
        /// This is a good method to use to warn about something that might be problematic, but can likely be ignored and app execution can continue without crashing.
        /// I.e. errors we can recover from.
        /// </summary>
        template<typename... Args>
        EXPORT static void Warn(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Warn);
        }

        /// <summary>
        /// Sends an error level msg to the log.
        /// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
        /// This is a good method to use to log errors that will likely cause issues during runtime.
        /// </summary>
        template<typename... Args>
        EXPORT static void Error(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            _dispatcher.Dispatch(msg, LogLevel::Error);
        }

        /// <summary>
        /// Sends a critical level msg to the log.
        /// If a log is created and listening the msg will be written to a log file in release, others a console in debug.
        /// This is a good method to use if the app is in a state where it cannot continue i.e. unrecoverable errors.
        /// </summary>
        template<typename... Args>
        EXPORT static void Critical(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
        }

    private:
        static inline WriteToLogEventDispatcher _dispatcher = {};
        static std::string _logFilePath;
        static bool _isOpen;
    };
}
