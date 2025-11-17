#pragma once
#include "tbx/messages/log_level.h"
#include "tbx/messages/command.h"
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
