#pragma once
#include "tbx/common/int.h"
#include "tbx/debugging/log_level.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string>

namespace tbx
{
    struct TBX_API LogMessageRequest : public Request<void>
    {
        LogMessageRequest(
            LogLevel lvl,
            const std::string& msg,
            const std::filesystem::path& file_path,
            uint32 line_num)
            : level(lvl)
            , message(msg)
            , file(file_path)
            , line(line_num)

        {
        }

        LogLevel level = LogLevel::INFO;
        std::string message;
        std::filesystem::path file;
        uint32 line = 0;
    };

}
