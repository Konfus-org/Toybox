#include "tbx/common/string_extensions.h"
#include <algorithm>
#include <cctype>

namespace tbx
{
    String trim_string(String_view text)
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
        return String(begin, end);
    }

    String to_lower_case_string(String_view text)
    {
        String lowered(text);
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
