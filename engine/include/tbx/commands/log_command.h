#pragma once
#include "tbx/commands/command.h"
#include <string>

namespace tbx
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error,
        Critical
    };

    struct LogMessageCommand : public Command
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;
        int line = 0;
    };
}

