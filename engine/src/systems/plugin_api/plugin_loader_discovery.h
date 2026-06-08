#pragma once
#include "tbx/interfaces/file_ops.h"
#include <filesystem>
#include <string>

namespace tbx
{
    TBX_API std::filesystem::path append_debug_postfix(const std::filesystem::path& library_path);

    TBX_API std::filesystem::path resolve_requested_plugin_library_path(
        const std::filesystem::path& directory,
        const std::string& plugin_name,
        const IFileOps& file_ops);

    TBX_API std::filesystem::path resolve_requested_plugin_meta_path(
        const std::filesystem::path& directory,
        const std::string& plugin_name,
        const IFileOps& file_ops);
}
