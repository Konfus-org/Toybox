#include "tbx/debugging/logging.h"
#include "tbx/files/filesystem.h"
#include <string>

namespace tbx
{
    const uint MAX_LOG_HISTORY = 10;

    static String sanitize_path(const String& base_name)
    {
        String sanitized_str = FilePath(base_name).get_filename();
        return sanitized_str;
    }

    static FilePath make_log_path(
        const FilePath& directory,
        const String& sanitized_base_name,
        int index)
    {
        const String stem = sanitized_base_name;
        return index <= 0 ? directory.append(stem + ".log")
                          : directory.append(stem + "_" + std::to_string(index) + ".log");
    }

    FilePath Log::open(IFileSystem& ops)
    {
        String sanitized = FilePath("Debug").get_filename();
        const FilePath root = ops.resolve_relative_path(ops.get_logs_directory());
        if (!ops.create_directory(root))
        {
            return FilePath();
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

        return make_log_path(ops.resolve_relative_path(sanitized), sanitized, 0);
    }

    String Log::format(const String& message)
    {
        return message;
    }

    String Log::format(const char* message)
    {
        if (message == nullptr)
        {
            return String();
        }

        return String(message);
    }

    void Log::post(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const String& message)
    {
        dispatcher.post<LogMessageRequest>(level, message, file, line);
    }
}
