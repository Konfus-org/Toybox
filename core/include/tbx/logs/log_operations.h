#pragma once
#include "tbx/file_system/filesystem_ops.h"
#include "tbx/tbx_api.h"
#include <filesystem>
#include <string_view>

namespace tbx::logs
{
    TBX_API std::filesystem::path calculate_log_path(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int index);

    TBX_API void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history,
        files::IFilesystemOps& ops);

    TBX_API void rotate_logs(
        const std::filesystem::path& directory,
        std::string_view base_name,
        int max_history = 10);
}
