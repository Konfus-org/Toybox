#pragma once
#include "tbx/tbx_api.h"
#include <string>
#include <string_view>

namespace tbx
{
    /// <summary>
    /// Returns a copy of the provided text without leading or trailing whitespace characters.
    /// Thread safe: the function does not access shared state and operates only on the provided input.
    /// </summary>
    TBX_API std::string trim_string(std::string_view text);

    /// <summary>
    /// Returns a lowercase copy of the provided text using the default C locale rules.
    /// Thread safe: the function does not access shared state and operates only on the provided input.
    /// </summary>
    TBX_API std::string to_lower_case_string(std::string_view text);
}
