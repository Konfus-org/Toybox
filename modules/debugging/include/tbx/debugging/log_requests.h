#pragma once
#include "tbx/debugging/log_level.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"
#include <cstdint>
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
            std::uint32_t line_num)
            : level(lvl)
            , message(msg)
            , file(file_path)
            , line(line_num)

        {
        }

        LogLevel level = LogLevel::Info;
        std::string message;
        std::filesystem::path file;
        std::uint32_t line = 0;
    };

}
