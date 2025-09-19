#pragma once
#include "Tbx/Debug/LogLevel.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/DllExport.h"
#include <format>
#include <memory>
#include <queue>
#include "Tbx/TypeAliases/Pointers.h"

namespace Tbx
{
    class Log
    {
    public:
        /// <summary>
        /// Opens the log.
        /// </summary>
        EXPORT static void Initialize(Tbx::Ref<ILogger> logger = nullptr);

        /// <summary>
        /// Closes the log.
        /// </summary>
        EXPORT static void Shutdown();

        /// <summary>
        /// Is the logger open and ready to log?
        /// </summary>
        EXPORT static bool IsOpen();

        /// <summary>
        /// Write a message to the log.
        /// </summary>
        EXPORT static void Write(LogLevel lvl, const std::string& msg);

        /// <summary>
        /// Write all queued messages to the log.
        /// </summary>
        EXPORT static void WriteQueued();

        /// <summary>
        /// Returns the path to the log file if we are logging to a file.
        /// If not returns an empty string.
        /// </summary>
        EXPORT static std::string GetFolderPath();

        /// <summary>
        /// Writes a trace level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to trace calls, ex: "MyCoolMethod called".
        /// </summary>
        template<typename... Args>
        EXPORT static void Trace(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Trace, msg);
        }

        /// <summary>
        /// Writes a info level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log info to the log that we want to track during runtime. Ex: window resize, layers attached, shutdown triggered, etc...
        /// </summary>
        template<typename... Args>
        EXPORT static void Info(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Info, msg);
        }

        /// <summary>
        /// Writes a debug level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log info that is intended to be used to track down a bug with the intention to be removed once the bug is solved.
        /// </summary>
        template<typename... Args>
        EXPORT static void Debug(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Debug, msg);
        }

        /// <summary>
        /// Writes a warning to the log.
        /// Msg will be written to a log file in release, a console in debug.
        /// This is a good method to use to warn about something that might be problematic, but can likely be ignored and app execution can continue without crashing.
        /// I.e. errors we can recover from.
        /// </summary>
        template<typename... Args>
        EXPORT static void Warn(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Warn, msg);
        }

        /// <summary>
        /// Sends an error level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log errors that will likely cause issues during runtime.
        /// </summary>
        template<typename... Args>
        EXPORT static void Error(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Error, msg);
        }

        /// <summary>
        /// Sends a critical level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use if the app is in a state where it cannot continue i.e. unrecoverable errors.
        /// </summary>
        template<typename... Args>
        EXPORT static void Critical(const std::string& fmt_str, Args&&... args)
        {
            auto msg = std::vformat(fmt_str, std::make_format_args(args...));
            Write(LogLevel::Critical, msg);
        }

    private:
        static std::queue<std::pair<LogLevel, std::string>> _logQueue;
        static Tbx::Ref<ILogger> _logger;
        static std::string _logFilePath;
        static bool _isOpen;
    };
}
