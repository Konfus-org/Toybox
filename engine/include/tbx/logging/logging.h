#pragma once
#include "tbx/commands/log_command.h"
#include "tbx/dispatch/dispatcher.h"
#include <sstream>

namespace tbx
{
    inline void submit_log(MessageDispatcher& dispatcher, LogLevel level, const char* file, int line, const std::string& message)
    {
        LogMessageCommand cmd;
        cmd.level = level;
        cmd.message = message;
        cmd.file = file ? std::string(file) : std::string();
        cmd.line = line;
        dispatcher.send(cmd);
    }
}
