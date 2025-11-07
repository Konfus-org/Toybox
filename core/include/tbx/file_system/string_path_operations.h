#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>

namespace tbx::files
{
    // Normalizes a string for safe usage as part of a file name by replacing
    // disallowed characters with underscores. The resulting string is never empty;
    // if the input is empty a default token is substituted.
    TBX_API std::string sanitize_for_file_name_usage(const std::string_view value);

    // Extracts only the filename component from a path (without directories).
    TBX_API std::string filename_only(const std::string_view path);
}
