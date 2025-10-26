#pragma once
#include "tbx/messages/commands/log_commands.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/dispatcher_context.h"
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace tbx
{
    inline const char* to_string(LogLevel lvl)
    {
        switch (lvl)
        {
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    inline void std_log(LogLevel level, const char* file, int line, const std::string& message, bool warn_if_first = false)
    {
        static bool warned = false;
        if (warn_if_first && !warned)
        {
            std::cout << "[TBX] No log handlers; falling back to stdout" << std::endl;
            warned = true;
        }
        std::cout << '[' << to_string(level) << "] "
                  << (file ? file : "<unknown>") << ':' << line << " - "
                  << message << std::endl;
    }

    inline void submit_log(IMessageDispatcher& dispatcher, LogLevel level, const char* file, int line, const std::string& message)
    {
        LogMessageCommand cmd;
        cmd.level = level;
        cmd.message = message;
        cmd.file = file ? std::string(file) : std::string();
        cmd.line = line;
        dispatcher.send(cmd);
        if (!cmd.is_handled)
        {
            std_log(level, file, line, message, true);
        }
    }

    // Convenience: uses the thread-local current dispatcher if one is set.
    inline void submit_log(LogLevel level, const char* file, int line, const std::string& message)
    {
        if (IMessageDispatcher* d = current_dispatcher())
        {
            submit_log(*d, level, file, line, message);
        }
        else
        {
            // No dispatcher set; fallback to stdout and warn once.
            std_log(level, file, line, message, true);
        }
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
        requires (sizeof...(Args) > 0)
    inline std::string format_log_message(std::string_view fmt, Args&&... args)
    {
        return std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
    }

    template <typename... Args>
    inline void submit_formatted(IMessageDispatcher& dispatcher, LogLevel level, const char* file, int line, Args&&... args)
    {
        ::tbx::submit_log(dispatcher, level, file, line,
            format_log_message(std::forward<Args>(args)...));
    }

    template <typename... Args>
    inline void submit_formatted(LogLevel level, const char* file, int line, Args&&... args)
    {
        ::tbx::submit_log(level, file, line,
            format_log_message(std::forward<Args>(args)...));
    }
}
