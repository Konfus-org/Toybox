#include "tbx/file_system/string_path_operations.h"
#include <string>

namespace tbx
{
    std::string sanitize_string_for_file_name_usage(std::string_view value)
    {
        static constexpr char replacement = '_';
        static constexpr std::string_view invalid = "<>:\"/\\|?*";

        std::string sanitized;
        sanitized.reserve(value.size());
        for (const unsigned char ch : value)
        {
            if (ch < 32 || invalid.find(static_cast<char>(ch)) != std::string_view::npos)
            {
                sanitized.push_back(replacement);
            }
            else
            {
                sanitized.push_back(static_cast<char>(ch));
            }
        }

        if (sanitized.empty())
        {
            sanitized = "unnamed";
        }

        return sanitized;
    }

    std::string get_filename_from_string_path(std::string_view path)
    {
        if (path.empty())
        {
            return {};
        }

        const size_t separator = path.find_last_of("/\\");
        if (separator == std::string_view::npos)
        {
            return std::string(path);
        }
        if (separator + 1 >= path.size())
        {
            return {};
        }
        return std::string(path.substr(separator + 1));
    }
}
