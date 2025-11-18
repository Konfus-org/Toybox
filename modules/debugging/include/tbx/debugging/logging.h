#pragma once
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/dispatcher_context.h"
#include "tbx/messages/log_commands.h"
#include "tbx/std/string.h"
#include <format>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace tbx
{
    inline String format_log_message(const String& message)
    {
        return message;
    }

    inline String format_log_message(std::string_view message)
    {
        return String(message.data(), static_cast<uint>(message.size()));
    }

    inline String format_log_message(const char* message)
    {
        return message ? String(message) : String();
    }

    template <typename... Args>
        requires(sizeof...(Args) > 0)
    String format_log_message(std::string_view fmt, Args&&... args)
    {
        // Pass arguments as lvalues to avoid binding rvalues to non-const references
        // inside std::make_format_args on some standard library implementations.
        auto arguments = std::make_tuple(std::forward<Args>(args)...);
        std::string formatted = std::apply(
            [&](auto&... tuple_args)
            {
                return std::vformat(fmt, std::make_format_args(tuple_args...));
            },
            arguments);
        return formatted;
    }

    inline void cout(
        LogLevel level,
        const char* file,
        int line,
        const String& message,
        bool warn_if_first = false)
    {
        static bool warned = false;
        if (warn_if_first && !warned)
        {
            std::cout << "[TBX] No log handlers; falling back to stdout\n";
            warned = true;
        }
        std::cout << '[' << to_string(level) << "] " << (file ? file : "<unknown>") << ':' << line
                  << " - " << message.get_raw() << "\n";
    }

    inline void trace(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const String& message)
    {
        LogMessageCommand cmd;
        cmd.level = level;
        cmd.message = message;
        cmd.file = file ? String(file) : String();
        cmd.line = line;
        auto result = dispatcher.send(cmd);
        if (!result)
        {
            cout(level, file, line, message, true);
        }
    }

    // Convenience: uses the thread-local current dispatcher if one is set.
    inline void trace(LogLevel level, const char* file, int line, const String& message)
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

    TBX_API std::filesystem::path calculate_log_path(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int index);

    TBX_API void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history,
        IFilesystemOps& ops);

    TBX_API void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history = 10);
}
