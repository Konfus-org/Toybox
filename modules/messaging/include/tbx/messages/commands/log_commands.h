#pragma once
#include "tbx/debug/log_level.h"
#include "tbx/messages/commands/command.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct TBX_API LogMessageCommand : public Command
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;
        int line = 0;
    };
}
