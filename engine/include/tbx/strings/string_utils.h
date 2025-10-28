#pragma once
#include <string>

namespace tbx
{
    // Removes leading and trailing whitespace characters from the provided string.
    std::string trim_string(const std::string& value);

    // Produces a lowercase copy of the supplied string using the C locale.
    std::string to_lower_case_string(const std::string& value);
}

