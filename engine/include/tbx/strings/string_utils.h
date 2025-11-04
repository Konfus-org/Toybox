#pragma once
#include "tbx/tbx_api.h"
#include <string>

namespace tbx
{
    // Removes leading and trailing whitespace characters from the provided string.
    TBX_API std::string trim_string(const std::string& value);

    // Produces a lowercase copy of the supplied string using the C locale.
    TBX_API std::string to_lower_case_string(const std::string& value);
}
