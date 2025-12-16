#pragma once
#include "tbx/common/string.h"
#include "tbx/debugging/log_level.h"
#include "tbx/file_system/filepath.h"
#include "tbx/messages/message.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    struct TBX_API LogMessageRequest : public Request<void>
    {
        LogMessageRequest(LogLevel lvl, const String& msg, const FilePath& file_path, uint line_num)
            : level(lvl)
            , message(msg)
            , file(file_path)
            , line(line_num)

        {
        }

        LogLevel level = LogLevel::Info;
        String message;
        FilePath file;
        uint line = 0;
    };

}
