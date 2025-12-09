#include "tbx/debugging/logging.h"
#include "tbx/file_system/filesystem.h"
#include <string>
#include <string_view>

namespace tbx
{
    static String sanitize_log_base_name(std::string_view base_name)
    {
        return FilePath(base_name).filename_string();
    }

    static FilePath make_log_path(
        const FilePath& directory,
        const String& sanitized_base_name,
        int index)
    {
        const std::string stem = sanitized_base_name.std_str();
        return index <= 0 ? directory.append(stem + ".log")
                          : directory.append(stem + "_" + std::to_string(index) + ".log");
    }

    FilePath get_log_file_path(
        const FilePath& directory,
        std::string_view base_name,
        int index,
        IFileSystem& ops)
    {
        const String sanitized = sanitize_log_base_name(base_name);
        return make_log_path(ops.resolve_relative_path(directory), sanitized, index);
    }

    void rotate_logs(
        const FilePath& directory,
        std::string_view base_name,
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

    std::string format_log_message(const std::string& message)
    {
        return message;
    }

    std::string format_log_message(std::string_view message)
    {
        return std::string(message);
    }

    std::string format_log_message(const char* message)
    {
        if (message == nullptr)
        {
            return std::string();
        }

        return std::string(message);
    }

    void post_log_msg(
        const IMessageDispatcher& dispatcher,
        LogLevel level,
        const char* file,
        int line,
        const std::string& message)
    {
        dispatcher.post<LogMessageRequest>(level, message, file, line);
    }
}
