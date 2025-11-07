#pragma once
#include "tbx/messages/commands/command.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    enum class TBX_API LogLevel
    {
        Info,
        Warning,
        Error,
        Critical
    };

    struct TBX_API LogMessageCommand : public Command
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;
        int line = 0;
    };
}
