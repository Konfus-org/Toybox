#pragma once
#include "tbx/messages/log_level.h"
#include "tbx/messages/command.h"
#include "tbx/std/string.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API LogMessageCommand : public Command
    {
        LogLevel level = LogLevel::Info;
        String message;
        String file;
        int line = 0;
    };
}
