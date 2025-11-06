#include "tbx/strings/mods.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    /// <summary>
    /// Removes leading and trailing whitespace characters from the input string.
    /// </summary>
    std::string trim_string(const std::string& value)
    {
        size_t begin = 0;
        size_t end = value.size();
        while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            begin += 1;
        }
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            end -= 1;
        }
        return value.substr(begin, end - begin);
    }

    /// <summary>
    /// Converts every character in the input string to lowercase.
    /// </summary>
    std::string to_lower_case_string(const std::string& value)
    {
        std::string lower = value;
        std::ranges::transform(
            lower,
            lower.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return lower;
    }
}
