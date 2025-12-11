#include "tbx/debugging/logging.h"
#include "tbx/file_system/filesystem.h"
#include <string>

namespace tbx
{
    static String sanitize_log_base_name(const String& base_name)
    {
        return FilePath(base_name).filename_string();
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

    FilePath get_log_file_path(
        const FilePath& directory,
        const String& base_name,
        int index,
        IFileSystem& ops)
    {
        const String sanitized = sanitize_log_base_name(base_name);
        return make_log_path(ops.resolve_relative_path(directory), sanitized, index);
    }

    void rotate_logs(
        const FilePath& directory,
        const String& base_name,
        int max_history,
        IFileSystem& ops)
    {
        if (max_history <= 0)
            return;

        const String sanitized = sanitize_log_base_name(base_name);
        const FilePath root = ops.resolve_relative_path(directory);
        ops.create_directory(root);

        for (int index = max_history; index >= 1; --index)
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
    }

    String format_log_message(const String& message)
    {
        return message;
    }

    String format_log_message(const char* message)
    {
        if (message == nullptr)
        {
            return String();
        }

        return String(message);
    }

    void post_log_msg(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const String& message)
    {
        dispatcher.post<LogMessageRequest>(level, message, file, line);
    }
}
