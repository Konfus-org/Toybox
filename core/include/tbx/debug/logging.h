#pragma once
#include "tbx/messages/commands/log_commands.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/dispatcher_context.h"
#include <format>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace tbx
{
    // Logging helpers that dispatch log messages through the current message
    // dispatcher when available, or write to stdout as a fallback.
    // Ownership: Non-owning; callers manage dispatcher and message lifetimes.
    // Thread-safety: Not inherently thread-safe; intended for use on the main
    // thread unless the dispatcher implementation provides concurrency.
    inline const char* to_string(LogLevel lvl)
    {
        switch (lvl)
        {
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warning:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
            case LogLevel::Critical:
                return "CRITICAL";
        }
        return "UNKNOWN";
    }

    inline std::string format_log_message(const std::string& message)
    {
        return message;
    }

    inline std::string format_log_message(std::string_view message)
    {
        return std::string(message);
    }

    inline std::string format_log_message(const char* message)
    {
        return message ? std::string(message) : std::string();
    }

    template <typename... Args>
        requires(sizeof...(Args) > 0)
    std::string format_log_message(std::string_view fmt, Args&&... args)
    {
        // Pass arguments as lvalues to avoid binding rvalues to non-const references
        // inside std::make_format_args on some standard library implementations.
        auto arguments = std::make_tuple(std::forward<Args>(args)...);
        return std::apply(
            [&](auto&... tuple_args)
            { return std::vformat(fmt, std::make_format_args(tuple_args...)); },
            arguments);
    }

    inline void cout(
        LogLevel level,
        const char* file,
        int line,
        const std::string& message,
        bool warn_if_first = false)
    {
        static bool warned = false;
        if (warn_if_first && !warned)
        {
            std::cout << "[TBX] No log handlers; falling back to stdout\n";
            warned = true;
        }
        std::cout << '[' << to_string(level) << "] " << (file ? file : "<unknown>") << ':' << line
                  << " - " << message << "\n";
    }

    inline void trace(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message)
    {
        LogMessageCommand cmd;
        cmd.level = level;
        cmd.message = message;
        cmd.file = file ? std::string(file) : std::string();
        cmd.line = line;
        auto result = dispatcher.send(cmd);
        (void)result;
        if (cmd.state != MessageState::Handled)
        {
            cout(level, file, line, message, true);
        }
    }

    // Convenience: uses the thread-local current dispatcher if one is set.
    inline void trace(LogLevel level, const char* file, int line, const std::string& message)
    {
        if (IMessageDispatcher* d = current_dispatcher())
        {
            trace(*d, level, file, line, message);
        }
        else
        {
            // No dispatcher set; fallback to stdout and warn once.
            cout(level, file, line, message, true);
        }
    }

    template <typename... Args>
    void trace(LogLevel level, const char* file, int line, std::string_view fmt, Args&&... args)
    {
        trace(level, file, line, format_log_message(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void trace(
        IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        std::string_view fmt,
        Args&&... args)
    {
        trace(dispatcher, level, file, line, format_log_message(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void trace_info(
        IMessageDispatcher& dispatcher,
        const std::source_location& loc,
        std::string_view fmt,
        Args&&... args)
    {
        trace(
            dispatcher,
            LogLevel::Info,
            loc.file_name(),
            static_cast<int>(loc.line()),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_warning(
        IMessageDispatcher& dispatcher,
        const std::source_location& loc,
        std::string_view fmt,
        Args&&... args)
    {
        trace(
            dispatcher,
            LogLevel::Warning,
            loc.file_name(),
            static_cast<int>(loc.line()),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_error(
        IMessageDispatcher& dispatcher,
        const std::source_location& loc,
        std::string_view fmt,
        Args&&... args)
    {
        trace(
            dispatcher,
            LogLevel::Error,
            loc.file_name(),
            static_cast<int>(loc.line()),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_critical(
        IMessageDispatcher& dispatcher,
        const std::source_location& loc,
        std::string_view fmt,
        Args&&... args)
    {
        trace(
            dispatcher,
            LogLevel::Critical,
            loc.file_name(),
            static_cast<int>(loc.line()),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_info(IMessageDispatcher& dispatcher, std::string_view fmt, Args&&... args)
    {
        trace_info(dispatcher, std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_warning(IMessageDispatcher& dispatcher, std::string_view fmt, Args&&... args)
    {
        trace_warning(
            dispatcher,
            std::source_location::current(),
            fmt,
            std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_error(IMessageDispatcher& dispatcher, std::string_view fmt, Args&&... args)
    {
        trace_error(dispatcher, std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void trace_critical(IMessageDispatcher& dispatcher, std::string_view fmt, Args&&... args)
    {
        trace_critical(
            dispatcher,
            std::source_location::current(),
            fmt,
            std::forward<Args>(args)...);
    }
}
