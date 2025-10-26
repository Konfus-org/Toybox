#pragma once
#include "tbx/messages/commands/log_command.h"
#include "tbx/messages/dispatcher.h"
#include "tbx/messages/dispatcher_context.h"
#include <iostream>
#include <sstream>

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
}
