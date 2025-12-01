#pragma once
#include "tbx/debugging/log_level.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    struct TBX_API LogMessageRequest : public Request<void>
    {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;
        int line = 0;
    };
}
