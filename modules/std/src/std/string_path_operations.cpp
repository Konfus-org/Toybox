#include "tbx/file_system/string_path_operations.h"
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

namespace tbx
{
    String sanitize_string_for_file_name_usage(const std::string_view value)
    {
        std::string result;
        result.reserve(value.size());

        char previous = '\0';
        for (unsigned char ch : value)
        {
            if (std::isalnum(ch) != 0 || ch == '-' || ch == '_')
            {
                result.push_back(static_cast<char>(ch));
            }
            else
            {
                if (!(ch == '\\' && previous == '\\'))
                {
                    result.push_back('_');
                }
            }

            previous = static_cast<char>(ch);
        }

        if (result.empty())
        {
            result = "unnamed";
        }

        return result;
    }

    String get_filename_from_string_path(const std::string_view path)
    {
        if (path.empty())
        {
            return {};
        }

        return std::filesystem::path(path).filename().string();
    }
}
