#include "tbx/debug/log.h"
#include <spdlog/spdlog.h>

namespace tbx
{
    //// LOGGING (spdlog backend) ////

    void write_log(const LogLevel level, const std::string_view message)
    {
        switch (level)
        {
            case LogLevel::TRACE:
                spdlog::trace(message);
                return;
            case LogLevel::INFO:
                spdlog::info(message);
                return;
            case LogLevel::WARN:
                spdlog::warn(message);
                return;
            case LogLevel::FAIL:
                spdlog::error(message);
                return;
        }
    }
}
