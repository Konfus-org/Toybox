#include "tbx/debug/log.h"
#include <spdlog/spdlog.h>

namespace tbx
{
    //// LOGGING (spdlog backend) ////

    void write_log(
        const LogLevel level,
        const std::string_view message,
        const std::string_view source_file,
        const int source_line)
    {
        // Just the file name — full build paths drown the actual message.
        const auto separator = source_file.find_last_of("/\\");
        const auto file_name = separator == std::string_view::npos
            ? source_file
            : source_file.substr(separator + 1);
        switch (level)
        {
            case LogLevel::TRACE:
                spdlog::trace("[{}:{}] {}", file_name, source_line, message);
                return;
            case LogLevel::INFO:
                spdlog::info("[{}:{}] {}", file_name, source_line, message);
                return;
            case LogLevel::WARN:
                spdlog::warn("[{}:{}] {}", file_name, source_line, message);
                return;
            case LogLevel::FAIL:
                spdlog::error("[{}:{}] {}", file_name, source_line, message);
                return;
        }
    }
}
