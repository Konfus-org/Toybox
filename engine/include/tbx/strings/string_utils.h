#pragma once
#include <string>

namespace tbx::strings
{
    /// <summary>
    /// Removes leading and trailing whitespace characters from the provided string.
    /// </summary>
    std::string trim(const std::string& value);
    /// <summary>
    /// Produces a lowercase copy of the supplied string using the C locale.
    /// </summary>
    std::string to_lower(const std::string& value);
}

