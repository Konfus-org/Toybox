#include "tbx/debugging/logging.h"
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/file_system/string_path_operations.h"
#include <filesystem>
#include <string>

namespace tbx
{
    static std::filesystem::path make_log_path(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int index)
    {
        const std::string sanitized = sanitize_string_for_file_name_usage(base_name);

        if (index <= 0)
        {
            return directory / (sanitized + ".log");
        }

        return directory / (sanitized + "_" + std::to_string(index) + ".log");
    }

    std::filesystem::path calculate_log_path(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int index)
    {
        return make_log_path(directory, base_name, index);
    }

    void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history,
        IFilesystemOps& ops)
    {
        if (max_history <= 0)
        {
            return;
        }

        for (int index = max_history; index >= 1; --index)
        {
            const auto from = make_log_path(directory, base_name, index - 1);
            const auto to = make_log_path(directory, base_name, index);

            if (!ops.exists(from))
            {
                continue;
            }

            if (ops.exists(to))
            {
                ops.remove(to);
            }

            if (ops.rename(from, to))
            {
                continue;
            }

            if (ops.copy(from, to))
            {
                ops.remove(from);
            }
        }
    }

    void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history)
    {
        rotate_logs(directory, base_name, max_history, get_default_filesystem_ops());
    }
}
