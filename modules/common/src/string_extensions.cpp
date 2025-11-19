#include "tbx/common/string_extensions.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    std::string trim_string(std::string_view text)
    {
        auto begin = text.begin();
        auto end = text.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)) != 0)
        {
            ++begin;
        }
        while (end != begin)
        {
            auto last = end - 1;
            if (std::isspace(static_cast<unsigned char>(*last)) == 0)
            {
                break;
            }
            end = last;
        }
        return std::string(begin, end);
    }

    std::string to_lower_case_string(std::string_view text)
    {
        std::string lowered(text);
        std::transform(
            lowered.begin(),
            lowered.end(),
            lowered.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return lowered;
    }
}
