#pragma once
#include "Tbx/Debug/IPrintable.h"
#include "Tbx/Debug/LogLevel.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/DllExport.h"
#include "Tbx/Memory/Refs.h"
#include <format>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Tbx
{
    class TBX_EXPORT Log
    {
    public:
        /// <summary>
        /// Sets a logger and opens it for writing
        /// </summary>
        static void SetLogger(Ref<ILogger> logger = nullptr);

        /// <summary>
        /// If we have a logger this clears it and also flushes any queued messages.
        /// </summary>
        static void ClearLogger();

        /// <summary>
        /// Write a message to the log.
        /// </summary>
        static void Write(LogLevel lvl, const std::string& msg);

        /// <summary>
        /// Write all queued messages to the log.
        /// </summary>
        static void Flush();

        /// <summary>
        /// Returns the path to the log file if we are logging to a file.
        /// If not returns an empty string.
        /// </summary>
        static std::string GetFilePath();

        /// <summary>
        /// Writes a trace level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to trace calls, ex: "MyCoolMethod called".
        /// </summary>
        template<typename... Args>
        static void Trace(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Trace, msg);
        }

        /// <summary>
        /// Writes a info level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log info to the log that we want to track during runtime. Ex: window resize, layers attached, shutdown triggered, etc...
        /// </summary>
        template<typename... Args>
        static void Info(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Info, msg);
        }

        /// <summary>
        /// Writes a debug level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log info that is intended to be used to track down a bug with the intention to be removed once the bug is solved.
        /// </summary>
        template<typename... Args>
        static void Debug(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Debug, msg);
        }

        /// <summary>
        /// Writes a warning to the log.
        /// Msg will be written to a log file in release, a console in debug.
        /// This is a good method to use to warn about something that might be problematic, but can likely be ignored and app execution can continue without crashing.
        /// I.e. errors we can recover from.
        /// </summary>
        template<typename... Args>
        static void Warn(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Warn, msg);
        }

        /// <summary>
        /// Sends an error level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use to log errors that will likely cause issues during runtime.
        /// </summary>
        template<typename... Args>
        static void Error(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Error, msg);
        }

        /// <summary>
        /// Sends a critical level msg to the log.
        /// Msg will be written to a log file in release and a console in debug.
        /// This is a good method to use if the app is in a state where it cannot continue i.e. unrecoverable errors.
        /// </summary>
        template<typename... Args>
        static void Critical(const std::string& fmt_str, Args&&... args)
        {
            auto msg = Format(fmt_str, std::forward<Args>(args)...);
            Write(LogLevel::Critical, msg);
        }

    private:
        template <typename T>
        static auto NormalizeFormatArg(T&& value)
        {
            if constexpr (std::is_base_of_v<IPrintable, std::decay_t<T>>)
            {
                return std::string(value.ToString());
            }
            else
            {
                return std::forward<T>(value);
            }
        }

        template <typename... Args>
        static std::string Format(const std::string& fmt_str, Args&&... args)
        {
            auto normalizedArgs = std::make_tuple(NormalizeFormatArg(std::forward<Args>(args))...);
            return std::apply(
                [&](auto&... values)
                {
                    return std::vformat(fmt_str, std::make_format_args(values...));
                },
                normalizedArgs);
        }

        static std::queue<std::pair<LogLevel, std::string>> _logQueue;
        static Ref<ILogger> _logger;
        static std::string _logFilePath;
    };
}
