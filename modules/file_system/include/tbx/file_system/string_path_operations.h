#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>

namespace tbx
{
    // Normalizes a string for safe usage as part of a file name by replacing
    // disallowed characters with underscores. The resulting string is never empty;
    // if the input is empty a default token is substituted.
    TBX_API std::string sanitize_string_for_file_name_usage(std::string_view value);

    // Extracts only the filename component from a path (without directories).
    TBX_API std::string get_filename_from_string_path(std::string_view path);
}
