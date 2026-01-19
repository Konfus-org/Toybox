#include "tbx/debugging/logging.h"
#include "tbx/files/filesystem.h"
#include <string>

namespace tbx
{
    const int MAX_LOG_HISTORY = 10;

    static std::filesystem::path make_log_path(
        const std::filesystem::path& directory,
        const std::string& sanitized_base_name,
        int index)
    {
        const std::string stem = sanitized_base_name;
        return index <= 0 ? directory / (stem + ".log")
                          : directory / (stem + "_" + std::to_string(index) + ".log");
    }

    std::filesystem::path Log::open(IFileSystem& ops)
    {
        std::string sanitized = std::filesystem::path("Debug").filename().string();
        const std::filesystem::path root =
            ops.resolve_relative_path(ops.get_logs_directory());
        if (!ops.create_directory(root))
        {
            return {};
        }

        for (int index = MAX_LOG_HISTORY; index >= 1; index--)
        {
            const auto from = make_log_path(root, sanitized, index - 1);
            const auto to = make_log_path(root, sanitized, index);

            if (!ops.exists(from))
                continue;
            if (ops.exists(to))
                ops.remove(to);
            if (ops.rename(from, to))
                continue;
            if (ops.copy(from, to))
                ops.remove(from);
        }

        return make_log_path(root, sanitized, 0);
    }

    std::string Log::format(std::string_view message)
    {
        return std::string(message);
    }

    std::string Log::format(const char* message)
    {
        if (message == nullptr)
        {
            return std::string();
        }

        return std::string(message);
    }

    void Log::post(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message)
    {
        dispatcher.post<LogMessageRequest>(level, message, file, line);
    }
}
